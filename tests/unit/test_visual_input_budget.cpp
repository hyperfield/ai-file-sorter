#include "VisualInputBudget.hpp"
#include "VisualLlmRuntime.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

TEST_CASE("Visual input budget reserves generation and counts tokens rather than positions", "[visual-budget]")
{
    CHECK(VisualInputBudget::fits(4016, 80, 4096));
    CHECK_FALSE(VisualInputBudget::fits(4017, 80, 4096));
    CHECK_FALSE(VisualInputBudget::fits(4148, 80, 4096));
    CHECK(VisualInputBudget::fits(170, 80, 4096)); // M-RoPE positions are not token slots.
    CHECK_FALSE(VisualInputBudget::fits(std::numeric_limits<std::size_t>::max(), 80, 4096));
    CHECK_FALSE(VisualInputBudget::fits(0, 4097, 4096));
}

TEST_CASE("Visual input budget preserves fitting image pixels and skips recounting", "[visual-budget]")
{
    QImage image(23, 19, QImage::Format_RGB888);
    image.fill(Qt::red);
    int calls = 0;
    const auto fitted = VisualInputBudget::fit(image, 1000, 80, 4096,
        [&](const QImage&) { ++calls; return 1000; });
    CHECK(calls == 0);
    CHECK(fitted.resize_count == 0);
    CHECK(fitted.image.cacheKey() == image.cacheKey());
    CHECK(fitted.image == image);
}

TEST_CASE("Visual input budget recounts quantized oversized images until the complete prompt fits", "[visual-budget]")
{
    QImage image(1600, 1600, QImage::Format_RGB888);
    image.fill(Qt::blue);
    const auto count = [](const QImage& candidate) {
        const auto patches = static_cast<std::size_t>((candidate.width() + 15) / 16) *
                             static_cast<std::size_t>((candidate.height() + 15) / 16);
        return 92 + std::min<std::size_t>(4096, patches);
    };
    const auto fitted = VisualInputBudget::fit(image, count(image), 80, 4096, count);
    CHECK(fitted.resize_count > 1); // Saturated native image limit needs actual size reduction.
    CHECK(VisualInputBudget::fits(fitted.prompt_tokens, 80, 4096));
    CHECK(fitted.prompt_tokens == count(fitted.image));
    CHECK(fitted.image.width() == fitted.image.height());
    CHECK(fitted.image.pixelColor(0, 0) == QColor(Qt::blue));
    CHECK(image.size() == QSize(1600, 1600));
}

TEST_CASE("Visual input budget bounds retries when text or a fixed projector cannot fit", "[visual-budget]")
{
    QImage image(1600, 1200, QImage::Format_RGB888);
    int calls = 0;
    try {
        VisualInputBudget::fit(image, 4200, 80, 4096,
            [&](const QImage&) { ++calls; return 4200; });
        FAIL("An unfit prompt was accepted");
    } catch (const std::runtime_error& error) {
        CHECK(calls > 0);
        CHECK(calls <= 12);
        CHECK_FALSE(VisualLlmRuntime::should_offer_cpu_fallback(error.what()));
    }
}

TEST_CASE("Visual input budget rejects invalid images and unavailable generation space", "[visual-budget]")
{
    const auto count = [](const QImage&) { return 1000; };
    CHECK_THROWS_AS(VisualInputBudget::fit({}, 1000, 80, 4096, count), std::invalid_argument);
    CHECK_THROWS_AS(VisualInputBudget::fit(QImage(1, 1, QImage::Format_RGB888), 1000, 4096, 4096, count), std::invalid_argument);
    CHECK_THROWS_AS(VisualInputBudget::fit(QImage(1, 1, QImage::Format_RGB888), 1000, 80, 4096, {}), std::invalid_argument);
}
