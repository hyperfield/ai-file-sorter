#include <catch2/catch_test_macros.hpp>

#include "LlavaImageAnalyzer.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"

#ifndef GGML_USE_METAL
namespace {

struct BackendProbeGuard {
    ~BackendProbeGuard() {
        TestHooks::reset_backend_memory_probe();
        TestHooks::reset_backend_availability_probe();
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
    CHECK(LlavaImageAnalyzerTestAccess::default_visual_batch_size(true, "cuda") == 768);
#endif
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
#endif
