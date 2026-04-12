#include "VisualLlmRuntime.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::optional<std::filesystem::path> resolve_artifact_path(
    const std::filesystem::path& primary,
    const VisualModelArtifactDescriptor& descriptor)
{
    if (std::filesystem::exists(primary)) {
        return primary;
    }

    const auto llm_dir = std::filesystem::path(Utils::get_default_llm_destination());
    for (const auto alt_name : descriptor.fallback_filenames) {
        const auto candidate = llm_dir / std::filesystem::path(std::string(alt_name));
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

} // namespace

std::optional<std::filesystem::path> VisualLlmRuntime::Backend::path_for(VisualModelArtifactKind kind) const
{
    const auto it =
        std::find_if(artifacts.begin(), artifacts.end(), [kind](const ResolvedArtifact& artifact) {
            return artifact.descriptor && artifact.descriptor->kind == kind;
        });
    if (it == artifacts.end()) {
        return std::nullopt;
    }
    return it->path;
}

bool VisualLlmRuntime::default_text_llm_files_available()
{
    static const char* kEnvVars[] = {
        "LOCAL_LLM_3B_DOWNLOAD_URL",
        "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL",
        "LOCAL_LLM_7B_DOWNLOAD_URL"
    };

    for (const char* env_key : kEnvVars) {
        const char* env_url = std::getenv(env_key);
        if (!env_url || *env_url == '\0') {
            continue;
        }

        try {
            const std::filesystem::path path =
                Utils::make_default_path_to_file_from_download_url(env_url);
            std::error_code ec;
            if (!path.empty() && std::filesystem::exists(path, ec)) {
                return true;
            }
        } catch (...) {
            continue;
        }
    }

    return false;
}

std::optional<VisualLlmRuntime::Backend> VisualLlmRuntime::resolve_active_backend(
    std::string_view backend_id,
    std::string* error)
{
    const VisualModelDescriptor* descriptor_ptr = nullptr;
    if (!backend_id.empty()) {
        descriptor_ptr = find_visual_model_descriptor(backend_id);
    }
    if (!descriptor_ptr) {
        descriptor_ptr = &default_visual_model_descriptor();
    }
    const auto& descriptor = *descriptor_ptr;
    std::vector<ResolvedArtifact> artifacts;
    artifacts.reserve(descriptor.artifacts.size());

    std::vector<const char*> missing_envs;
    for (const auto& artifact : descriptor.artifacts) {
        const char* env_url = std::getenv(artifact.url_env);
        if (!env_url || !*env_url) {
            missing_envs.push_back(artifact.url_env);
        }
    }

    if (!missing_envs.empty()) {
        if (error) {
            std::string message = "Missing visual LLM download URLs. Check ";
            for (size_t i = 0; i < missing_envs.size(); ++i) {
                if (i > 0) {
                    message += " and ";
                }
                message += missing_envs[i];
            }
            message.push_back('.');
            *error = std::move(message);
        }
        return std::nullopt;
    }

    for (const auto& artifact : descriptor.artifacts) {
        const char* env_url = std::getenv(artifact.url_env);
        std::filesystem::path primary_path;
        try {
            primary_path = std::filesystem::path(
                Utils::make_default_path_to_file_from_download_url(env_url));
        } catch (...) {
            if (error) {
                *error = "Failed to resolve visual LLM file paths.";
            }
            return std::nullopt;
        }

        const auto resolved_path = resolve_artifact_path(primary_path, artifact);
        if (!resolved_path) {
            if (error) {
                if (artifact.kind == VisualModelArtifactKind::Model) {
                    *error = "Visual LLM model file is missing: " + primary_path.string();
                } else {
                    *error = "Visual LLM mmproj file is missing: " + primary_path.string();
                }
            }
            return std::nullopt;
        }

        artifacts.push_back(ResolvedArtifact{&artifact, *resolved_path});
    }

    return Backend{&descriptor, std::move(artifacts)};
}

std::optional<VisualLlmRuntime::Paths> VisualLlmRuntime::resolve_paths(std::string_view backend_id,
                                                                       std::string* error)
{
    const auto backend = resolve_active_backend(backend_id, error);
    if (!backend.has_value()) {
        return std::nullopt;
    }

    const auto model_path = backend->path_for(VisualModelArtifactKind::Model);
    const auto mmproj_path = backend->path_for(VisualModelArtifactKind::Mmproj);
    if (!model_path.has_value() || !mmproj_path.has_value()) {
        if (error) {
            *error = "Resolved visual backend is missing required model/mmproj artifacts.";
        }
        return std::nullopt;
    }

    return Paths{*model_path, *mmproj_path};
}

bool VisualLlmRuntime::should_use_gpu()
{
    const char* backend = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
    if (!backend || !*backend) {
        return true;
    }
    return to_lower_copy(backend) != "cpu";
}

bool VisualLlmRuntime::should_offer_cpu_fallback(const std::string& reason)
{
    const std::string lowered = to_lower_copy(reason);
    static const char* kRetryableMarkers[] = {
        "failed to create llama_context",
        "mtmd_helper_eval_chunks failed",
        "out of memory",
        "not enough memory",
        "gpu memory",
        "vk_error_out_of_device_memory",
        "vk_error_out_of_host_memory",
        "cuda error out of memory",
        "cuda_error_out_of_memory"
    };

    for (const char* marker : kRetryableMarkers) {
        if (lowered.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}
