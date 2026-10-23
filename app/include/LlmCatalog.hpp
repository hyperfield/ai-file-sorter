#ifndef LLMCATALOG_HPP
#define LLMCATALOG_HPP

#include "Settings.hpp"

#include <QString>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Metadata for default local LLM downloads and labels.
 */
struct DefaultLlmEntry {
    LLMChoice choice;
    const char* url_env;
    const char* name_env;
    const char* fallback_name;
    /** @brief Optional visual backend that can share this model's text weights. */
    const char* visual_backend_id{nullptr};
    /** @brief URL setting used to verify that the shared artifacts match. */
    const char* visual_model_url_env{nullptr};
};

/**
 * @brief Returns the default local LLM entries in priority order.
 * @return List of default local LLM entries.
 */
const std::vector<DefaultLlmEntry>& default_llm_entries();

/**
 * @brief Returns metadata for a built-in local LLM choice.
 * @param choice Built-in local LLM identifier.
 * @return Matching entry, or `nullptr` when the choice is not a built-in local model.
 */
const DefaultLlmEntry* default_llm_entry_for_choice(LLMChoice choice);

/**
 * @brief Builds the UI label for a default local LLM entry.
 * @param entry Default LLM entry definition.
 * @return Localized label for display in UI/benchmark output.
 */
QString default_llm_label(const DefaultLlmEntry& entry);

/**
 * @brief Builds the UI label for a default local LLM choice.
 * @param choice LLM choice identifier.
 * @return Localized label, or a generic label when not found.
 */
QString default_llm_label_for_choice(LLMChoice choice);

/**
 * @brief Returns the primary download-url environment variable for a built-in local LLM.
 * @param choice Built-in local LLM identifier.
 * @return Environment variable name, or an empty string when unsupported.
 */
std::string default_llm_download_env_var_for_choice(LLMChoice choice);

/**
 * @brief Returns the preferred on-disk artifact path for a built-in local LLM choice.
 * @param choice Built-in local LLM identifier.
 * @return Preferred artifact path, or an empty path when the choice is unsupported.
 */
std::filesystem::path preferred_builtin_llm_path(LLMChoice choice);

/**
 * @brief Resolves an already-downloaded artifact path for a built-in local LLM choice.
 *
 * Legacy choices may probe historical artifact filenames in addition to the current preferred one.
 *
 * @param choice Built-in local LLM identifier.
 * @return Existing artifact path when found; otherwise `std::nullopt`.
 */
std::optional<std::filesystem::path> resolve_downloaded_builtin_llm_path(LLMChoice choice);

/**
 * @brief Returns whether a built-in local LLM artifact is already present on disk.
 * @param choice Built-in local LLM identifier.
 * @return True when any supported artifact for the choice exists locally.
 */
bool builtin_llm_artifact_available(LLMChoice choice);

#endif // LLMCATALOG_HPP
