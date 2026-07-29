#ifndef CATEGORIZATION_RESPONSE_PARSER_HPP
#define CATEGORIZATION_RESPONSE_PARSER_HPP

#include <optional>
#include <string>
#include <utility>

namespace CategorizationResponseParser {

/**
 * @brief Result of validating category and subcategory labels.
 */
struct LabelValidationResult {
    bool valid{false};
    std::string error;
};

/**
 * @brief Parse a model response into sanitized category and subcategory labels.
 * @param input Raw LLM categorization response.
 * @return Sanitized category/subcategory pair; either value may be empty if parsing fails.
 */
std::pair<std::string, std::string> split_category_subcategory(const std::string& input);

/**
 * @brief Parse a translation response containing category/subcategory labels.
 * @param response Raw LLM translation response, preferably JSON but tolerant of plain pairs.
 * @return Sanitized translated labels, or std::nullopt when no category can be parsed.
 */
std::optional<std::pair<std::string, std::string>> parse_translated_category_response(
    const std::string& response);

/**
 * @brief Validate category and subcategory labels before filesystem persistence.
 * @param category Main category label.
 * @param subcategory Subcategory label.
 * @return Validation result with a user/log-facing error on failure.
 */
LabelValidationResult validate_labels(const std::string& category,
                                      const std::string& subcategory);

} // namespace CategorizationResponseParser

#endif // CATEGORIZATION_RESPONSE_PARSER_HPP
