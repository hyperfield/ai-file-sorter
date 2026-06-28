#include "VisualModelCatalog.hpp"

#include "GgufFileValidation.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <fstream>

namespace {

constexpr std::string_view kLegacyDefaultLlavaBackendId = "llava-v1.6-mistral-7b";
constexpr std::string_view kCustomVisualModelIdPrefix = "custom:";

std::optional<std::string> read_cached_download_url(const std::filesystem::path& candidate)
{
    std::ifstream in(candidate.string() + ".aifs.meta");
    if (!in.is_open()) {
        return std::nullopt;
    }

    std::string line;
    constexpr const char kUrlPrefix[] = "url=";
    while (std::getline(in, line)) {
        if (line.rfind(kUrlPrefix, 0) == 0) {
            return line.substr(sizeof(kUrlPrefix) - 1);
        }
    }

    return std::nullopt;
}

bool is_shared_legacy_filename(const std::filesystem::path& candidate)
{
    const auto filename = candidate.filename().string();
    return filename == "mmproj-model.gguf" || filename == "mmproj-model-f16.gguf";
}

bool is_default_llava_mmproj_without_metadata_compatible(
    const VisualModelDescriptor& backend,
    const VisualModelArtifactDescriptor& artifact,
    const std::filesystem::path& candidate)
{
    if (artifact.kind != VisualModelArtifactKind::Mmproj || !is_shared_legacy_filename(candidate)) {
        return false;
    }

    return std::string_view(backend.id) == kLegacyDefaultLlavaBackendId;
}

bool accept_legacy_candidate(const VisualModelDescriptor& backend,
                             const VisualModelArtifactDescriptor& artifact,
                             const std::filesystem::path& candidate,
                             std::string_view download_url)
{
    if (!has_gguf_header(candidate)) {
        return false;
    }

    const auto cached_url = read_cached_download_url(candidate);
    if (cached_url.has_value()) {
        return std::string_view(*cached_url) == download_url;
    }

    return !is_shared_legacy_filename(candidate)
        || is_default_llava_mmproj_without_metadata_compatible(backend, artifact, candidate);
}

std::optional<std::filesystem::path> legacy_path_from_url(std::string_view download_url)
{
    if (download_url.empty()) {
        return std::nullopt;
    }

    try {
        return std::filesystem::path(
            Utils::make_default_path_to_file_from_download_url(std::string(download_url)));
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

const std::vector<VisualModelDescriptor>& visual_model_descriptors()
{
    static const std::vector<VisualModelDescriptor> descriptors = {
        {
            "llava-v1.6-mistral-7b",
            "LLaVA 1.6 Mistral 7B",
            VisualModelArchitecture::MtmdProjector,
            VisualPromptPolicy::LegacyLlava,
            {
                {VisualModelArtifactKind::Model,
                 "LLaVA 1.6 Mistral 7B (text model)",
                 "LLAVA_MODEL_URL",
                 "model.gguf",
                 {}},
                {VisualModelArtifactKind::Mmproj,
                 "LLaVA mmproj (vision encoder)",
                 "LLAVA_MMPROJ_URL",
                 "mmproj.gguf",
                 {"mmproj-model-f16.gguf",
                  "llava-v1.6-mistral-7b-mmproj-f16.gguf"}},
            },
        },
        {
            "gemma-3-4b-it",
            "Gemma 3 4B IT",
            VisualModelArchitecture::MtmdProjector,
            VisualPromptPolicy::StructuredVisionInstruct,
            {
                {VisualModelArtifactKind::Model,
                 "Gemma 3 4B IT (text model)",
                 "GEMMA3_4B_MODEL_URL",
                 "model.gguf",
                 {}},
                {VisualModelArtifactKind::Mmproj,
                 "Gemma 3 4B mmproj (vision encoder)",
                 "GEMMA3_4B_MMPROJ_URL",
                 "mmproj.gguf",
                 {"mmproj-model.gguf",
                  "mmproj-model-f16.gguf",
                  "mmproj-gemma-3-4b-it-Q4_0.gguf",
                  "mmproj-gemma-3-4b-it-Q4_K_M.gguf",
                  "mmproj-gemma-3-4b-it-Q8_0.gguf"}},
            },
        },
    };
    return descriptors;
}

const VisualModelDescriptor* find_visual_model_descriptor(std::string_view id)
{
    const auto& descriptors = visual_model_descriptors();
    const auto it = std::find_if(descriptors.begin(), descriptors.end(),
                                 [id](const VisualModelDescriptor& descriptor) {
                                     return descriptor.id == id;
                                 });
    if (it == descriptors.end()) {
        return nullptr;
    }
    return &(*it);
}

const VisualModelDescriptor& default_visual_model_descriptor()
{
    const auto* gemma = find_visual_model_descriptor("gemma-3-4b-it");
    if (gemma) {
        return *gemma;
    }
    return visual_model_descriptors().front();
}

const VisualModelDescriptor& custom_visual_model_descriptor()
{
    static const VisualModelDescriptor descriptor = {
        "custom",
        "Custom visual LLM",
        VisualModelArchitecture::MtmdProjector,
        VisualPromptPolicy::StructuredVisionInstruct,
        {
            {VisualModelArtifactKind::Model,
             "Custom visual text model",
             "",
             "",
             {}},
            {VisualModelArtifactKind::Mmproj,
             "Custom mmproj",
             "",
             "",
             {}},
        },
    };
    return descriptor;
}

std::string custom_visual_model_id_for_llm(std::string_view custom_llm_id)
{
    if (custom_llm_id.empty()) {
        return {};
    }
    std::string id(kCustomVisualModelIdPrefix);
    id.append(custom_llm_id);
    return id;
}

std::optional<std::string> custom_llm_id_from_visual_model_id(std::string_view visual_model_id)
{
    if (visual_model_id.rfind(kCustomVisualModelIdPrefix, 0) != 0) {
        return std::nullopt;
    }
    const auto id = visual_model_id.substr(kCustomVisualModelIdPrefix.size());
    if (id.empty()) {
        return std::nullopt;
    }
    return std::string(id);
}

bool is_custom_visual_model_id(std::string_view visual_model_id)
{
    return custom_llm_id_from_visual_model_id(visual_model_id).has_value();
}

std::filesystem::path visual_artifact_storage_path(const VisualModelDescriptor& backend,
                                                   const VisualModelArtifactDescriptor& artifact)
{
    return std::filesystem::path(Utils::get_default_llm_destination())
           / std::filesystem::path(backend.id)
           / std::filesystem::path(artifact.local_storage_name);
}

std::optional<std::filesystem::path> resolve_visual_artifact_path(
    const VisualModelDescriptor& backend,
    const VisualModelArtifactDescriptor& artifact,
    std::string_view download_url)
{
    const auto preferred_path = visual_artifact_storage_path(backend, artifact);
    if (std::filesystem::exists(preferred_path) && has_gguf_header(preferred_path)) {
        return preferred_path;
    }

    if (const auto legacy_path = legacy_path_from_url(download_url);
        legacy_path.has_value() && std::filesystem::exists(*legacy_path)
        && accept_legacy_candidate(backend, artifact, *legacy_path, download_url)) {
        return legacy_path;
    }

    const auto llm_dir = std::filesystem::path(Utils::get_default_llm_destination());
    for (const auto fallback_name : artifact.fallback_filenames) {
        const auto candidate = llm_dir / std::filesystem::path(std::string(fallback_name));
        if (!std::filesystem::exists(candidate)) {
            continue;
        }
        if (accept_legacy_candidate(backend, artifact, candidate, download_url)) {
            return candidate;
        }
    }

    return std::nullopt;
}
