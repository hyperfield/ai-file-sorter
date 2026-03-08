#include <catch2/catch_test_macros.hpp>

#include "MainAppTestAccess.hpp"

#include <string>

TEST_CASE("Visual CPU fallback detection recognizes retryable GPU failures") {
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Failed to create llama_context (try AI_FILE_SORTER_VISUAL_USE_GPU=0 to force CPU)"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback("mtmd_helper_eval_chunks failed"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback("VK_ERROR_OUT_OF_DEVICE_MEMORY"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback("CUDA error out of memory"));
}

TEST_CASE("Visual CPU fallback detection ignores non-retryable startup failures") {
    CHECK_FALSE(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Failed to load mmproj file at C:/models/mmproj.gguf"));
    CHECK_FALSE(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Failed to load LLaVA text model at C:/models/model.gguf"));
    CHECK_FALSE(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "The provided mmproj file does not expose vision capabilities"));
}
