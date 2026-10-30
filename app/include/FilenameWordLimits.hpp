#pragma once

#include <algorithm>

inline constexpr int kMinimumFilenameWords = 3;
inline constexpr int kMaximumFilenameWords = 6;
inline constexpr int kDefaultFilenameWords = kMinimumFilenameWords;

/**
 * @brief Constrains a content-derived filename word limit to the supported range.
 * @param words Requested maximum word count.
 * @return A maximum between three and six words.
 */
inline constexpr int clamp_filename_words(int words)
{
    return std::clamp(words, kMinimumFilenameWords, kMaximumFilenameWords);
}
