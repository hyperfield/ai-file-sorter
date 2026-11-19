#pragma once

#include "Types.hpp"

#include <string>
#include <string_view>
#include <functional>
#include <optional>

namespace LocalLLMPromptBuilder {

/** @brief Combine content evidence with routing or classification context without source metadata. */
std::string build_request_context(const std::string& evidence, const std::string& context);

/** @brief True for existing-folder comparisons/reconsideration, excluding separate proposals. */
bool is_existing_folder_selection(std::string_view context);

/**
 * @brief Build the system prompt for local categorization.
 * @param file_path Prompt path/context, optionally including image or document analysis text.
 * @param file_type File or directory classification target.
 * @param consistency_context Optional prompt context used to infer refined vs. consistent taxonomy guidance.
 * @return System prompt tailored to the target type and available context.
 */
std::string build_system_prompt(std::string_view file_path,
                                FileType file_type,
                                std::string_view consistency_context = {});

/**
 * @brief Build the user prompt for local categorization.
 * @param file_name Display file or directory name used for categorization.
 * @param file_path Full path/context, optionally including image or document analysis text.
 * @param file_type File or directory classification target.
 * @param consistency_context Optional whitelist and learning context to include in the prompt.
 * @return User prompt sent to the local LLM.
 */
std::string build_user_prompt(const std::string& file_name,
                              const std::string& file_path,
                              FileType file_type,
                              const std::string& consistency_context);

/**
 * @brief Calculate the token budget available for prompt input.
 * @param context_tokens Runtime context size.
 * @param max_tokens Requested generation tokens.
 * @return Number of tokens reserved for the prompt after generation reserve.
 */
int prompt_token_budget(int context_tokens, int max_tokens);

/**
 * @brief Shrink a user prompt for progressively smaller context retry attempts.
 * @param prompt Original prompt text.
 * @param attempt Zero-based retry attempt index.
 * @return Shortened analysis text, retaining complete folder-routing constraints.
 */
std::string shrink_user_prompt_for_context(const std::string& prompt, int attempt);

/**
 * @brief Fit optional evidence while preserving the complete chat framing and constraints.
 * @param prompt Original user prompt.
 * @param token_budget Input token budget, excluding generation reserve.
 * @param count_tokens Counter for the full system/template/user prompt, not just user text.
 * @return Fitted user text, or empty when mandatory instructions cannot fit.
 */
std::optional<std::string> fit_user_prompt(const std::string& prompt, std::size_t token_budget,
    const std::function<std::size_t(const std::string&)>& count_tokens);

} // namespace LocalLLMPromptBuilder
