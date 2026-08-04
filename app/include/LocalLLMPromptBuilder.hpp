#pragma once

#include "Types.hpp"

#include <string>
#include <string_view>

namespace LocalLLMPromptBuilder {

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
 * @return Prompt with long document/image sections or the whole prompt truncated.
 */
std::string shrink_user_prompt_for_context(const std::string& prompt, int attempt);

} // namespace LocalLLMPromptBuilder
