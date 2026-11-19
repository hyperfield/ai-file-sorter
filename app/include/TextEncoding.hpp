#pragma once

#include <optional>
#include <string>
#include <string_view>

/** @brief Locale-independent protocol operations and strict UTF-8 label policy. */
namespace TextEncoding {
/** @brief Lowercase an ASCII byte, leaving UTF-8 bytes untouched. @param ch Byte to map. */
constexpr unsigned char ascii_lower(unsigned char ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; }
/** @brief Uppercase an ASCII byte, leaving UTF-8 bytes untouched. @param ch Byte to map. */
constexpr unsigned char ascii_upper(unsigned char ch) { return ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch; }
/** @brief Test an ASCII letter. @param ch Byte to classify. */
constexpr bool ascii_alpha(unsigned char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); }
/** @brief Test an ASCII decimal digit. @param ch Byte to classify. */
constexpr bool ascii_digit(unsigned char ch) { return ch >= '0' && ch <= '9'; }
/** @brief Test an ASCII letter or digit. @param ch Byte to classify. */
constexpr bool ascii_alnum(unsigned char ch) { return ascii_alpha(ch) || ascii_digit(ch); }
/** @brief Test ASCII protocol whitespace. @param ch Byte to classify. */
constexpr bool ascii_space(unsigned char ch) { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }
/** @brief Parse a complete finite decimal using the classic locale. @param text Protocol/config number. */
std::optional<double> parse_number(std::string_view text);
/** @brief Test for Unicode letters in strictly valid UTF-8. @param text UTF-8 text. */
bool contains_letter(std::string_view text);
/**
 * @brief Enforce 80 extended grapheme clusters and native filesystem component limits.
 * @param text Strict UTF-8 label; combining sequences count as one visible character.
 * @return False for malformed UTF-8, over 80 clusters, or over 255 UTF-16 units
 * on Windows / 255 UTF-8 bytes on POSIX. Character/path safety is checked separately.
 */
bool label_within_limits(std::string_view text);
/** @brief Lowercase Unicode letters/digits/attached marks with safe underscore separators. @param text UTF-8 place name. */
std::string place_slug(std::string_view text);
}
