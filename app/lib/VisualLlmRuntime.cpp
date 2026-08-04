#include "LlmCatalog.hpp"
#include "VisualLlmRuntime.hpp"

#include "GgufFileValidation.hpp"
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

std::optional<std::filesystem::path> custom_visual_path_from_utf8(const std::string& value,
                                                                  const char* label,
                                                                  std::string* error)
{
    if (value.empty()) {
        if (error) {
            *error = std::string("Custom visual LLM ") + label + " file is not configured.";
        }
        return std::nullopt;
    }

    std::filesystem::path path;
    try {
        path = Utils::utf8_to_path(value);
    } catch (const std::exception& ex) {
        if (error) {
            *error = std::string("Custom visual LLM ") + label
                + " path is invalid UTF-8: " + ex.what();
        }
        return std::nullopt;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (error) {
            *error = std::string("Custom visual LLM ") + label + " file is missing: "
                + Utils::path_to_utf8(path);
        }
        return std::nullopt;
    }
    if (!has_gguf_header(path)) {
        if (error) {
            *error = std::string("Custom visual LLM ") + label
                + " file is invalid or incomplete: " + Utils::path_to_utf8(path);
        }
        return std::nullopt;
    }
    return path;
}

const CustomLLM* find_custom_visual_llm(std::string_view id,
                                        const std::vector<CustomLLM>& custom_llms)
{
    const auto it = std::find_if(custom_llms.begin(),
                                 custom_llms.end(),
                                 [id](const CustomLLM& llm) {
                                     return llm.id == id;
                                 });
    if (it == custom_llms.end()) {
        return nullptr;
    }
    return &(*it);
}

std::optional<VisualLlmRuntime::Backend> resolve_custom_visual_backend(
    std::string_view backend_id,
    const std::vector<CustomLLM>& custom_llms,
    std::string* error)
{
    const auto custom_id = custom_llm_id_from_visual_model_id(backend_id);
    if (!custom_id.has_value()) {
        return std::nullopt;
    }

    const CustomLLM* custom = find_custom_visual_llm(*custom_id, custom_llms);
    if (!custom || !is_valid_custom_llm(*custom)) {
        if (error) {
            *error = "Selected custom visual LLM is missing or invalid. Please re-select it.";
        }
        return std::nullopt;
    }
    if (!is_visual_custom_llm(*custom)) {
        if (error) {
            *error = "Custom visual LLM requires both a model file and an MMProj file. "
                     "Edit the custom LLM entry and add an MMProj file.";
        }
        return std::nullopt;
    }

    const auto model_path = custom_visual_path_from_utf8(custom->path, "model", error);
    if (!model_path.has_value()) {
        return std::nullopt;
    }
    const auto mmproj_path = custom_visual_path_from_utf8(custom->mmproj_path, "MMProj", error);
    if (!mmproj_path.has_value()) {
        return std::nullopt;
    }

    const auto& descriptor = custom_visual_model_descriptor();
    if (descriptor.artifacts.size() < 2) {
        if (error) {
            *error = "Custom visual backend descriptor is incomplete.";
        }
        return std::nullopt;
    }

    std::vector<VisualLlmRuntime::ResolvedArtifact> artifacts;
    artifacts.push_back({&descriptor.artifacts[0], *model_path});
    artifacts.push_back({&descriptor.artifacts[1], *mmproj_path});
    return VisualLlmRuntime::Backend{&descriptor, std::move(artifacts)};
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
    for (const auto& entry : default_llm_entries()) {
        if (builtin_llm_artifact_available(entry.choice)) {
            return true;
        }
    }

    return false;
}

std::optional<VisualLlmRuntime::Backend> VisualLlmRuntime::resolve_active_backend(
    std::string_view backend_id,
    std::string* error)
{
    static const std::vector<CustomLLM> no_custom_llms;
    return resolve_active_backend(backend_id, no_custom_llms, error);
}

std::optional<VisualLlmRuntime::Backend> VisualLlmRuntime::resolve_active_backend(
    std::string_view backend_id,
    const std::vector<CustomLLM>& custom_llms,
    std::string* error)
{
    if (is_custom_visual_model_id(backend_id)) {
        return resolve_custom_visual_backend(backend_id, custom_llms, error);
    }

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
        const auto preferred_path = visual_artifact_storage_path(descriptor, artifact);
        const auto resolved_path = resolve_visual_artifact_path(descriptor, artifact, env_url);
        if (!resolved_path) {
            if (error) {
                if (artifact.kind == VisualModelArtifactKind::Model) {
                    *error = "Visual LLM model file is missing: " + preferred_path.string();
                } else {
                    *error = "Visual LLM mmproj file is missing: " + preferred_path.string();
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
    static const std::vector<CustomLLM> no_custom_llms;
    return resolve_paths(backend_id, no_custom_llms, error);
}

std::optional<VisualLlmRuntime::Paths> VisualLlmRuntime::resolve_paths(
    std::string_view backend_id,
    const std::vector<CustomLLM>& custom_llms,
    std::string* error)
{
    const auto backend = resolve_active_backend(backend_id, custom_llms, error);
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
        "gpu preflight crashed",
        "visual gpu preflight crashed",
        "visual gpu preflight timed out",
        "visual gpu preflight subprocess did not start",
        "0xc0000409",
        "gpu memory",
        "vk::device::allocatememory",
        "erroroutofdevicememory",
        "erroroutofhostmemory",
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
