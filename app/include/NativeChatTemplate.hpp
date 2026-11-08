#pragma once

#include <string>
#include <string_view>

struct llama_model;

namespace NativeChatTemplate {

/**
 * @brief Render the model's embedded chat template with thinking disabled.
 * @param model Loaded model; must not be null.
 * @param system_prompt System instructions, optionally empty.
 * @param user_prompt Complete user message, including any media marker.
 * @return Generation-ready prompt, or plain text when no template is embedded.
 * @throws std::exception If the model or embedded template is invalid.
 */
std::string format(const llama_model* model, std::string_view system_prompt,
                   std::string_view user_prompt);

/**
 * @brief Render an explicit Jinja template using the same native inference policy.
 * @param source Jinja chat template source.
 * @param system_prompt System instructions, optionally empty.
 * @param user_prompt Complete user message.
 * @return Generation-ready prompt without optional thinking output.
 * @throws std::exception If template initialization or rendering fails.
 */
std::string render(std::string_view source, std::string_view system_prompt,
                   std::string_view user_prompt);

} // namespace NativeChatTemplate
