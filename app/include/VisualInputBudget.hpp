/** @file VisualInputBudget.hpp @brief Fit visual inputs to a complete decoder token budget. */
#pragma once
#include <QImage>
#include <cstddef>
#include <functional>

namespace VisualInputBudget {
/** @brief Image and exact native prompt count after fitting, including unchanged inputs. */
struct FitResult {
    QImage image;
    std::size_t prompt_tokens = 0;
    std::size_t resize_count = 0;
};
/** @brief Count complete native prompt tokens without image encoding or decoder evaluation. */
using TokenCounter = std::function<std::size_t(const QImage&)>;
/**
 * @brief Check token slots, reserving generation without arithmetic overflow.
 * @param prompt_tokens All image and text tokens, not M-RoPE temporal positions.
 * @param output_tokens Maximum generated tokens.
 * @param context_tokens Actual initialized decoder capacity.
 * @return True when the complete request fits.
 */
bool fits(std::size_t prompt_tokens, std::size_t output_tokens, std::size_t context_tokens);
/**
 * @brief Resize only oversized inputs and recount complete prompts after bounded steps.
 * @param image Original decoded image; untouched when it fits.
 * @param prompt_tokens Exact native count for the original complete prompt.
 * @param output_tokens Maximum generated tokens.
 * @param context_tokens Actual initialized decoder capacity.
 * @param count_tokens Native tokenizer callback for resized images.
 * @return Image and count that fit, preserving aspect ratio.
 * @throws std::invalid_argument Invalid image, counter or generation space.
 * @throws std::runtime_error Input cannot fit after bounded resizing.
 */
FitResult fit(const QImage& image, std::size_t prompt_tokens, std::size_t output_tokens,
              std::size_t context_tokens, const TokenCounter& count_tokens);
} // namespace VisualInputBudget
