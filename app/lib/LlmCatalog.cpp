#include "LlmCatalog.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"

#include <QObject>

#include <algorithm>
#include <cstdlib>
#include <system_error>
#include <vector>

namespace {
constexpr char kLegacyLlama3BQ4Url[] =
    "https://huggingface.co/Mungert/Llama-3.2-3B-Instruct-GGUF/resolve/main/"
    "Llama-3.2-3B-Instruct-bf16-q4_k.gguf";

struct SharedVisualArtifact {
    const VisualModelDescriptor* backend;
    const VisualModelArtifactDescriptor* artifact;
    std::string download_url;
};

QString resolved_llm_name(const DefaultLlmEntry& entry)
{
    const char* env_value = std::getenv(entry.name_env);
    if (env_value && *env_value) {
        return QString::fromUtf8(env_value);
    }
    return QString::fromUtf8(entry.fallback_name);
}

std::optional<std::filesystem::path> path_from_url(const std::string& url)
{
    if (url.empty()) {
        return std::nullopt;
    }
    try {
        return std::filesystem::path(Utils::make_default_path_to_file_from_download_url(url));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::filesystem::path> path_from_env_var(const std::string& env_var)
{
    if (env_var.empty()) {
        return std::nullopt;
    }
    const char* env_value = std::getenv(env_var.c_str());
    if (!env_value || *env_value == '\0') {
        return std::nullopt;
    }
    return path_from_url(env_value);
}

std::optional<std::string> string_from_env_var(const std::string& env_var)
{
    if (env_var.empty()) {
        return std::nullopt;
    }
    const char* env_value = std::getenv(env_var.c_str());
    if (!env_value || *env_value == '\0') {
        return std::nullopt;
    }
    return std::string(env_value);
}

void append_unique_path(std::vector<std::filesystem::path>& paths,
                        const std::optional<std::filesystem::path>& candidate)
{
    if (!candidate || candidate->empty()) {
        return;
    }
    if (std::find(paths.begin(), paths.end(), *candidate) == paths.end()) {
        paths.push_back(*candidate);
    }
}

bool urls_reference_same_artifact(std::string_view left, std::string_view right)
{
    if (left.empty() || right.empty()) {
        return false;
    }
    if (left == right) {
        return true;
    }

    try {
        return Utils::get_file_name_from_url(std::string(left))
            == Utils::get_file_name_from_url(std::string(right));
    } catch (...) {
        return false;
    }
}

const VisualModelArtifactDescriptor* find_visual_model_artifact(
    const VisualModelDescriptor& backend,
    VisualModelArtifactKind kind)
{
    const auto artifact_it = std::find_if(
        backend.artifacts.begin(),
        backend.artifacts.end(),
        [kind](const VisualModelArtifactDescriptor& artifact) {
            return artifact.kind == kind;
        });
    if (artifact_it == backend.artifacts.end()) {
        return nullptr;
    }
    return &(*artifact_it);
}

std::optional<SharedVisualArtifact> shared_visual_artifact_for_builtin(LLMChoice choice)
{
    if (choice != LLMChoice::Local_4b_Gemma) {
        return std::nullopt;
    }

    const auto local_url = string_from_env_var(default_llm_download_env_var_for_choice(choice));
    const auto visual_url = string_from_env_var("GEMMA3_4B_MODEL_URL");
    if (!local_url || !visual_url || !urls_reference_same_artifact(*local_url, *visual_url)) {
        return std::nullopt;
    }

    const auto* backend = find_visual_model_descriptor("gemma-3-4b-it");
    if (!backend) {
        return std::nullopt;
    }

    const auto* artifact = find_visual_model_artifact(*backend, VisualModelArtifactKind::Model);
    if (!artifact) {
        return std::nullopt;
    }

    return SharedVisualArtifact{backend, artifact, *visual_url};
}

void append_shared_visual_model_path(std::vector<std::filesystem::path>& paths, LLMChoice choice)
{
    const auto shared_artifact = shared_visual_artifact_for_builtin(choice);
    if (!shared_artifact) {
        return;
    }

    append_unique_path(paths,
                       resolve_visual_artifact_path(*shared_artifact->backend,
                                                    *shared_artifact->artifact,
                                                    shared_artifact->download_url));
}

std::vector<std::filesystem::path> candidate_builtin_llm_paths(LLMChoice choice)
{
    std::vector<std::filesystem::path> paths;
    append_shared_visual_model_path(paths, choice);
    append_unique_path(paths, path_from_env_var(default_llm_download_env_var_for_choice(choice)));
    if (choice == LLMChoice::Local_3b_legacy) {
        append_unique_path(paths, path_from_url(kLegacyLlama3BQ4Url));
    }
    return paths;
}

bool path_exists(const std::filesystem::path& path)
{
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}
} // namespace

const std::vector<DefaultLlmEntry>& default_llm_entries()
{
    static const std::vector<DefaultLlmEntry> entries = {
        {LLMChoice::Local_4b_Gemma, "LOCAL_LLM_3B_DOWNLOAD_URL", "LOCAL_LLM_3B_DISPLAY_NAME",
         "Gemma 3 4B IT Q4_K_M"},
        {LLMChoice::Local_7b, "LOCAL_LLM_7B_DOWNLOAD_URL", "LOCAL_LLM_7B_DISPLAY_NAME",
         "Mistral 7B Instruct v0.2 Q5_K_M"},
        {LLMChoice::Local_7b_Gemma, "LOCAL_LLM_7B_GEMMA_DOWNLOAD_URL",
         "LOCAL_LLM_7B_GEMMA_DISPLAY_NAME", "Gemma 1.1 7B IT Q5_K_M"},
        {LLMChoice::Local_3b_legacy, "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL",
         "LOCAL_LLM_3B_LEGACY_DISPLAY_NAME", "LLaMa 3b v3.2 Instruct Q8, legacy"}};
    return entries;
}

const DefaultLlmEntry* default_llm_entry_for_choice(LLMChoice choice)
{
    const auto& entries = default_llm_entries();
    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [choice](const DefaultLlmEntry& entry) {
                                     return entry.choice == choice;
                                 });
    if (it == entries.end()) {
        return nullptr;
    }
    return &(*it);
}

QString default_llm_label(const DefaultLlmEntry& entry)
{
    return QObject::tr("Local LLM (%1)").arg(resolved_llm_name(entry));
}

QString default_llm_label_for_choice(LLMChoice choice)
{
    const DefaultLlmEntry* entry = default_llm_entry_for_choice(choice);
    if (!entry) {
        return QObject::tr("Local LLM");
    }
    return default_llm_label(*entry);
}

std::string default_llm_download_env_var_for_choice(LLMChoice choice)
{
    const DefaultLlmEntry* entry = default_llm_entry_for_choice(choice);
    if (!entry || !entry->url_env) {
        return {};
    }
    return entry->url_env;
}

std::filesystem::path preferred_builtin_llm_path(LLMChoice choice)
{
    const auto shared_artifact = shared_visual_artifact_for_builtin(choice);
    if (shared_artifact) {
        return visual_artifact_storage_path(*shared_artifact->backend, *shared_artifact->artifact);
    }

    const auto path = path_from_env_var(default_llm_download_env_var_for_choice(choice));
    if (!path) {
        return {};
    }
    return *path;
}

std::optional<std::filesystem::path> resolve_downloaded_builtin_llm_path(LLMChoice choice)
{
    for (const auto& candidate : candidate_builtin_llm_paths(choice)) {
        if (path_exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool builtin_llm_artifact_available(LLMChoice choice)
{
    return resolve_downloaded_builtin_llm_path(choice).has_value();
}
