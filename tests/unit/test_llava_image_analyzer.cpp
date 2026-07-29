#include <catch2/catch_test_macros.hpp>

#include "LlavaImageAnalyzer.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>

#ifndef GGML_USE_METAL
namespace {

struct BackendProbeGuard {
    ~BackendProbeGuard() {
        TestHooks::reset_backend_memory_probe();
        TestHooks::reset_backend_availability_probe();
        TestHooks::reset_cuda_memory_probe();
        TestHooks::reset_cuda_availability_probe();
    }
};

} // namespace
#endif

TEST_CASE("LlavaImageAnalyzer uses conservative default visual batch sizing") {
    CHECK(LlavaImageAnalyzerTestAccess::default_visual_batch_size(false, "vulkan") == 512);
    CHECK(LlavaImageAnalyzerTestAccess::default_visual_batch_size(true, "metal") == 1024);
    CHECK(LlavaImageAnalyzerTestAccess::default_visual_batch_size(true, "vulkan") == 512);
#if defined(_WIN32)
    CHECK(LlavaImageAnalyzerTestAccess::default_visual_batch_size(true, "cuda") == 512);
#else
    CHECK(LlavaImageAnalyzerTestAccess::default_visual_batch_size(true, "cuda") == 512);
#endif
}

TEST_CASE("LlavaImageAnalyzer builds a descending visual GPU-layer retry ladder") {
    const std::vector<int32_t> actual =
        LlavaImageAnalyzerTestAccess::visual_gpu_layer_retry_candidates(20);
    const std::vector<int32_t> expected = {20, 15, 11, 8, 6, 4, 3, 2, 1};

    CHECK(actual == expected);
}

TEST_CASE("LlavaImageAnalyzer keeps a single visual GPU-layer retry candidate at one layer") {
    const std::vector<int32_t> actual =
        LlavaImageAnalyzerTestAccess::visual_gpu_layer_retry_candidates(1);
    const std::vector<int32_t> expected = {1};

    CHECK(actual == expected);
}

TEST_CASE("LlavaImageAnalyzer keeps guarded visual projectors on CPU when headroom is tight") {
    constexpr std::uintmax_t mmproj_size = 624451168ULL;
    constexpr size_t tight_free = 1024ULL * 1024ULL * 1024ULL;
    constexpr size_t comfortable_free = 2ULL * 1024ULL * 1024ULL * 1024ULL;

    CHECK_FALSE(LlavaImageAnalyzerTestAccess::should_use_mmproj_gpu_for_memory(
        "cuda", tight_free, mmproj_size));
    CHECK_FALSE(LlavaImageAnalyzerTestAccess::should_use_mmproj_gpu_for_memory(
        "vulkan", tight_free, mmproj_size));
    CHECK(LlavaImageAnalyzerTestAccess::should_use_mmproj_gpu_for_memory(
        "cuda", comfortable_free, mmproj_size));
    CHECK(LlavaImageAnalyzerTestAccess::should_use_mmproj_gpu_for_memory(
        "metal", tight_free, mmproj_size));
}

TEST_CASE("LlavaImageAnalyzer exposes legacy LLaVA prompt policy") {
    CHECK(LlavaImageAnalyzerTestAccess::description_system_prompt(
              VisualPromptPolicy::LegacyLlava).empty());
    CHECK(LlavaImageAnalyzerTestAccess::filename_system_prompt(
              VisualPromptPolicy::LegacyLlava).empty());

    const auto description_prompt =
        LlavaImageAnalyzerTestAccess::description_user_prompt(VisualPromptPolicy::LegacyLlava);
    CHECK(description_prompt.find("Image: <__media__>") != std::string::npos);
    CHECK(description_prompt.find("Description:") != std::string::npos);

    const auto filename_prompt = LlavaImageAnalyzerTestAccess::filename_user_prompt(
        VisualPromptPolicy::LegacyLlava, "A photo of a sunset over the mountains.");
    CHECK(filename_prompt.find("Filename:") != std::string::npos);
    CHECK(filename_prompt.find("sunset_over_mountains") != std::string::npos);
}

TEST_CASE("LlavaImageAnalyzer exposes structured multimodal prompt policy") {
    const auto description_system = LlavaImageAnalyzerTestAccess::description_system_prompt(
        VisualPromptPolicy::StructuredVisionInstruct);
    CHECK(description_system.find("file organization") != std::string::npos);

    const auto description_prompt =
        LlavaImageAnalyzerTestAccess::description_user_prompt(
            VisualPromptPolicy::StructuredVisionInstruct);
    CHECK(description_prompt.find("<__media__>") != std::string::npos);
    CHECK(description_prompt.find("Output only the description.") != std::string::npos);

    const auto filename_system = LlavaImageAnalyzerTestAccess::filename_system_prompt(
        VisualPromptPolicy::StructuredVisionInstruct);
    CHECK(filename_system.find("filesystem-safe filename stems") != std::string::npos);

    const auto filename_prompt = LlavaImageAnalyzerTestAccess::filename_user_prompt(
        VisualPromptPolicy::StructuredVisionInstruct,
        "Invoice with handwritten totals and customer notes.");
    CHECK(filename_prompt.find("maximum 3 words") != std::string::npos);
    CHECK(filename_prompt.find("lowercase letters only") != std::string::npos);
    CHECK(filename_prompt.find("Output only the filename stem.") != std::string::npos);
}

TEST_CASE("LlavaImageAnalyzer supports WebP files for visual routing") {
    CHECK(LlavaImageAnalyzer::is_supported_image("/tmp/photo.webp"));
    CHECK(LlavaImageAnalyzer::is_supported_image("/tmp/photo.WEBP"));
    CHECK_FALSE(LlavaImageAnalyzer::is_supported_image("/tmp/photo.txt"));
}

TEST_CASE("LlavaImageAnalyzer decodes WebP through runtime fallback for visual analysis") {
    static constexpr std::array<unsigned char, 42> kTinyWebp = {
        0x52, 0x49, 0x46, 0x46, 0x22, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42,
        0x50, 0x56, 0x50, 0x38, 0x20, 0x16, 0x00, 0x00, 0x00, 0x30, 0x01,
        0x00, 0x9d, 0x01, 0x2a, 0x01, 0x00, 0x01, 0x00, 0x0e, 0xc0, 0xfe,
        0x25, 0xa4, 0x00, 0x03, 0x70, 0x00, 0x00, 0x00, 0x00
    };

    TempDir temp;
    const auto webp_path = temp.path() / "tiny.webp";
    std::ofstream out(webp_path, std::ios::binary);
    REQUIRE(out);
    out.write(reinterpret_cast<const char*>(kTinyWebp.data()),
              static_cast<std::streamsize>(kTinyWebp.size()));
    out.close();

    CHECK(LlavaImageAnalyzerTestAccess::can_transcode_webp_for_visual_analysis(webp_path));
}

#ifndef GGML_USE_METAL
TEST_CASE("LlavaImageAnalyzer ignores global GPU layer override by default") {
    TempModelFile model(48, 8 * 1024 * 1024);
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard global_override("AI_FILE_SORTER_N_GPU_LAYERS", "30");
    EnvVarGuard visual_override("AI_FILE_SORTER_VISUAL_N_GPU_LAYERS", std::nullopt);
    EnvVarGuard llama_override("LLAMA_CPP_N_GPU_LAYERS", std::nullopt);
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;

    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        TestHooks::BackendMemoryInfo info;
        info.memory.free_bytes = 3ULL * 1024ULL * 1024ULL * 1024ULL;
        info.memory.total_bytes = 3ULL * 1024ULL * 1024ULL * 1024ULL;
        info.is_integrated = false;
        info.name = "Visual Test GPU";
        return info;
    });

    const int32_t actual =
        LlavaImageAnalyzerTestAccess::visual_model_n_gpu_layers_for_model(model.path().string());
    CHECK(actual != 30);
    CHECK(actual > 0);
}

TEST_CASE("LlavaImageAnalyzer honors visual-specific GPU layer override") {
    TempModelFile model(48, 8 * 1024 * 1024);
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard global_override("AI_FILE_SORTER_N_GPU_LAYERS", "30");
    EnvVarGuard visual_override("AI_FILE_SORTER_VISUAL_N_GPU_LAYERS", "12");
    EnvVarGuard llama_override("LLAMA_CPP_N_GPU_LAYERS", std::nullopt);
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;

    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        return std::nullopt;
    });

    const int32_t actual =
        LlavaImageAnalyzerTestAccess::visual_model_n_gpu_layers_for_model(model.path().string());
    CHECK(actual == 12);
}

#ifdef AI_FILE_SORTER_HAS_MTMD
TEST_CASE("LlavaImageAnalyzer lowers visual ngl when reserving mmproj headroom") {
    TempModelFile model(34, 2489757856ULL);
    TempModelFile mmproj(1, 624451168ULL);

    const int32_t actual = LlavaImageAnalyzerTestAccess::visual_model_n_gpu_layers_with_headroom(
        model.path().string(),
        mmproj.path().string(),
        "cuda",
        3676ULL * 1024ULL * 1024ULL,
        3768ULL * 1024ULL * 1024ULL);

    CHECK(actual == 20);
}

TEST_CASE("LlavaImageAnalyzer reconciles visual ngl to the GPU tier when the reserve-aware estimate is only slightly lower") {
    TempModelFile model(34, 2489757856ULL);
    TempModelFile mmproj(1, 624451168ULL);

    const int32_t actual = LlavaImageAnalyzerTestAccess::visual_model_n_gpu_layers_with_headroom(
        model.path().string(),
        mmproj.path().string(),
        "cuda",
        3400ULL * 1024ULL * 1024ULL,
        3768ULL * 1024ULL * 1024ULL);

    CHECK(actual == 20);
}
#endif
#endif
