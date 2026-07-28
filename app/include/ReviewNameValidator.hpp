#ifndef REVIEW_NAME_VALIDATOR_HPP
#define REVIEW_NAME_VALIDATOR_HPP

#include <string>

namespace ReviewNameValidator {

/**
 * @brief Trim leading and trailing ASCII whitespace from text.
 * @param value Text to trim.
 * @return Trimmed copy of the input text.
 */
std::string trim_copy(const std::string& value);

/**
 * @brief Return whether a category label represents an absent category.
 * @param value Category or subcategory label.
 * @return True for an empty label or the sentinel `Uncategorized`.
 */
bool is_missing_category_label(const std::string& value);

/**
 * @brief Remove UI prefixes from stored analysis descriptions.
 * @param value Description text from the review row.
 * @return Description without known image/document prefixes.
 */
std::string strip_history_description_label(std::string value);

/**
 * @brief Validate a category/subcategory pair used for destination folders.
 * @param category Category folder label.
 * @param subcategory Subcategory folder label.
 * @param error Receives a human-readable validation error on failure.
 * @param allow_identical True to allow category and subcategory labels to match.
 * @return True when both labels are valid.
 */
bool validate_labels(const std::string& category,
                     const std::string& subcategory,
                     std::string& error,
                     bool allow_identical = false);

/**
 * @brief Validate a destination filename suggested in the review dialog.
 * @param name Filename to validate.
 * @param error Receives a human-readable validation error on failure.
 * @return True when the filename is safe for local filesystem use.
 */
bool validate_filename(const std::string& name, std::string& error);

} // namespace ReviewNameValidator

#endif // REVIEW_NAME_VALIDATOR_HPP
