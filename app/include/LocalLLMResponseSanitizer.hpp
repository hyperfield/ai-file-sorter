#pragma once

#include <string>

namespace LocalLLMResponseSanitizer {

/**
 * @brief Sanitizes local LLM categorization output into a parser-friendly form.
 * @param output Raw local model response.
 * @return A cleaned category string, preferably in `<Main category> : <Subcategory>` form.
 */
std::string sanitize_categorization_output(std::string output);

} // namespace LocalLLMResponseSanitizer
