#include "VisualInputBudget.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
// Patch quantization and a saturated native image limit can defeat proportional scaling.
// Reduce each dimension by at least a quarter to guarantee progress at that limit.
constexpr double kMaximumResizeRatio = 0.75;
constexpr double kTokenRoundingReserveRatio = 0.95;
constexpr std::size_t kMaximumResizeAttempts = 12;

QSize next_size(const QImage& image, std::size_t tokens, std::size_t available)
{
    const double ratio = std::min(kMaximumResizeRatio, kTokenRoundingReserveRatio *
        std::sqrt(static_cast<double>(available) / static_cast<double>(tokens)));
    return {std::max(1, static_cast<int>(image.width() * ratio)),
            std::max(1, static_cast<int>(image.height() * ratio))};
}
} // namespace

bool VisualInputBudget::fits(std::size_t prompt_tokens, std::size_t output_tokens,
                             std::size_t context_tokens)
{
    return output_tokens <= context_tokens && prompt_tokens <= context_tokens - output_tokens;
}

VisualInputBudget::FitResult VisualInputBudget::fit(const QImage& image, std::size_t prompt_tokens,
    std::size_t output_tokens, std::size_t context_tokens, const TokenCounter& count_tokens)
{
    if (image.isNull() || !count_tokens || output_tokens >= context_tokens) {
        throw std::invalid_argument("Visual input has no usable image or prompt budget");
    }
    FitResult result{image, prompt_tokens, 0};
    while (!fits(result.prompt_tokens, output_tokens, context_tokens)) {
        if (result.resize_count == kMaximumResizeAttempts) {
            throw std::runtime_error("Visual input cannot fit in the model context after bounded resizing");
        }
        const auto size = next_size(result.image, result.prompt_tokens, context_tokens - output_tokens);
        if (size == result.image.size()) {
            throw std::runtime_error("Visual input cannot fit in the model context at minimum image size");
        }
        result.image = result.image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (result.image.isNull()) {
            throw std::runtime_error("Could not resize visual input for the model context");
        }
        ++result.resize_count;
        result.prompt_tokens = count_tokens(result.image);
    }
    return result;
}
