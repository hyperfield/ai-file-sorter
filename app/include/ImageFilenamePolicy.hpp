#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

/** @brief Shared filename rules for local and HTTP image analyzers. */
namespace ImageFilenamePolicy {
inline constexpr size_t kMaxFilenameLength = 50;

/**
 * @brief Convert model output to a safe Unicode filename stem.
 * @param value Model response or fallback description.
 * @param max_words Maximum number of distinct meaningful words.
 * @param max_length Maximum stem length in UTF-16 code units.
 * @return Lowercase underscore-separated stem, or empty if no usable words remain.
 */
std::string sanitize_filename(const std::string& value, size_t max_words,
                              size_t max_length = kMaxFilenameLength);

/**
 * @brief Convert an original filename stem to a fallback slug.
 * @param value Original stem.
 * @return Safe nonempty slug.
 */
std::string slugify(const std::string& value);

/**
 * @brief Append the original extension to a suggested stem.
 * @param base Suggested stem.
 * @param original_path Original image path.
 * @return Suggested filename preserving the original extension.
 */
std::string normalize_filename(const std::string& base, const std::filesystem::path& original_path);
} // namespace ImageFilenamePolicy
