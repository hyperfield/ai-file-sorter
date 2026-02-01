#ifndef LLMCATALOG_HPP
#define LLMCATALOG_HPP

#include "Settings.hpp"

#include <QString>
#include <vector>

/**
 * @brief Metadata for default local LLM downloads and labels.
 */
struct DefaultLlmEntry {
    LLMChoice choice;
    const char* url_env;
    const char* name_env;
    const char* fallback_name;
};

/**
 * @brief Returns the default local LLM entries in priority order.
 * @return List of default local LLM entries.
 */
const std::vector<DefaultLlmEntry>& default_llm_entries();

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

#endif // LLMCATALOG_HPP
