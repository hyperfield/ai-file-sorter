#pragma once

#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Helpers for generated date suffixes shown in category folder names.
 *
 * Date suffixes are a reversible display/move-path overlay. They should not be
 * persisted as canonical category names.
 */
namespace CategoryDateSuffix {

/**
 * @brief Supported generated date suffix formats.
 */
enum class Kind {
    Image,    ///< Image capture dates use YYYY-MM-DD.
    Document, ///< Document creation dates use YYYY-MM.
};

/**
 * @brief Appends a generated date suffix unless the category already has it.
 * @param category Base category text.
 * @param date Date text without the leading underscore.
 * @return Category text with the generated date suffix.
 */
std::string append_date_suffix(std::string_view category, std::string_view date);

/**
 * @brief Removes a specific generated date suffix when present.
 * @param category Category text that may contain a generated suffix.
 * @param date Date text without the leading underscore.
 * @return Category text without the matching generated suffix.
 */
std::string strip_date_suffix(std::string_view category, std::string_view date);

/**
 * @brief Removes a generated date suffix matching the expected kind.
 * @param category Category text that may contain a generated suffix.
 * @param kind Expected generated date suffix kind.
 * @return Category text without the suffix, or std::nullopt when none matches.
 */
std::optional<std::string> strip_generated_suffix(std::string_view category, Kind kind);

} // namespace CategoryDateSuffix
