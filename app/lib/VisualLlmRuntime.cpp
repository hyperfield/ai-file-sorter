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

std::optional<std::filesystem::path> resolve_mmproj_path(const std::filesystem::path& primary)
{
    if (std::filesystem::exists(primary)) {
        return primary;
    }

    const auto llm_dir = std::filesystem::path(Utils::get_default_llm_destination());
    static const char* kAltMmprojNames[] = {
        "mmproj-model-f16.gguf",
        "llava-v1.6-mistral-7b-mmproj-f16.gguf"
    };
    for (const char* alt_name : kAltMmprojNames) {
        const auto candidate = llm_dir / alt_name;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

} // namespace

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

std::optional<VisualLlmRuntime::Paths> VisualLlmRuntime::resolve_paths(std::string* error)
{
    const char* model_url = std::getenv("LLAVA_MODEL_URL");
    const char* mmproj_url = std::getenv("LLAVA_MMPROJ_URL");
    if (!model_url || !*model_url || !mmproj_url || !*mmproj_url) {
        if (error) {
            *error = "Missing visual LLM download URLs. Check LLAVA_MODEL_URL and LLAVA_MMPROJ_URL.";
        }
        return std::nullopt;
    }

    std::filesystem::path model_path;
    std::filesystem::path mmproj_primary;
    try {
        model_path = std::filesystem::path(Utils::make_default_path_to_file_from_download_url(model_url));
        mmproj_primary = std::filesystem::path(Utils::make_default_path_to_file_from_download_url(mmproj_url));
    } catch (...) {
        if (error) {
            *error = "Failed to resolve visual LLM file paths.";
        }
        return std::nullopt;
    }

    if (!std::filesystem::exists(model_path)) {
        if (error) {
            *error = "Visual LLM model file is missing: " + model_path.string();
        }
        return std::nullopt;
    }

    const auto mmproj_path = resolve_mmproj_path(mmproj_primary);
    if (!mmproj_path) {
        if (error) {
            *error = "Visual LLM mmproj file is missing: " + mmproj_primary.string();
        }
        return std::nullopt;
    }

    return Paths{model_path, *mmproj_path};
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
