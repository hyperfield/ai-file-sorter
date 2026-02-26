#include <catch2/catch_test_macros.hpp>
#include "LocalLLMClient.hpp"
#include "LocalLLMTestAccess.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#ifndef GGML_USE_METAL

namespace {

struct CudaProbeGuard {
    ~CudaProbeGuard() {
        TestHooks::reset_cuda_availability_probe();
        TestHooks::reset_cuda_memory_probe();
    }
};

struct BackendProbeGuard {
    ~BackendProbeGuard() {
        TestHooks::reset_backend_memory_probe();
        TestHooks::reset_backend_availability_probe();
    }
};

} // namespace

TEST_CASE("detect_preferred_backend reads environment") {
    EnvVarGuard guard("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    REQUIRE(LocalLLMTestAccess::detect_preferred_backend() ==
            LocalLLMTestAccess::BackendPreference::Cuda);
}

TEST_CASE("CPU backend is honored when forced") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cpu");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
}

TEST_CASE("CUDA backend can be forced off via GGML_DISABLE_CUDA") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", "1");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    CudaProbeGuard guard;
    TestHooks::set_cuda_availability_probe([] { return true; });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
}

TEST_CASE("CUDA override is applied when backend is available") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "7");
    CudaProbeGuard guard;
    TestHooks::set_cuda_availability_probe([] { return true; });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 7);
}

TEST_CASE("CUDA fallback when no GPU is available") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    CudaProbeGuard guard;
    TestHooks::set_cuda_availability_probe([] { return false; });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE((params.n_gpu_layers == 0 || params.n_gpu_layers == -1));
}

TEST_CASE("Vulkan backend honors explicit override") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "12");
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        return std::nullopt;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 12);
}

TEST_CASE("Vulkan backend derives layer count from memory probe") {
    TempModelFile model(48, 8 * 1024 * 1024);
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
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
        info.name = "Vulkan Test GPU";
        return info;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers > 0);
    REQUIRE(params.n_gpu_layers <= 48);
}

TEST_CASE("Vulkan backend falls back to CPU when memory metrics are unavailable") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        return std::nullopt;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
}

TEST_CASE("Vulkan backend falls back to CPU when unavailable") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return false;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
}

TEST_CASE("LocalLLMClient declines GPU fallback when callback returns false") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "1");
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });

    bool called = false;
    try {
        LocalLLMClient client(model.path().string(),
                              [&called](const std::string&) {
                                  called = true;
                                  return false;
                              });
        FAIL("Expected LocalLLMClient to throw when CPU fallback is declined");
    } catch (const std::runtime_error& ex) {
        REQUIRE(called);
        REQUIRE(std::string(ex.what()).find("CPU fallback was declined") != std::string::npos);
    }
}

TEST_CASE("LocalLLMClient retries on CPU when fallback is accepted") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "1");
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });

    bool called = false;
    try {
        LocalLLMClient client(model.path().string(),
                              [&called](const std::string&) {
                                  called = true;
                                  return true;
                              });
        FAIL("Expected LocalLLMClient to throw due to invalid model");
    } catch (const std::runtime_error& ex) {
        REQUIRE(called);
        REQUIRE(std::string(ex.what()).find("Failed to load model") != std::string::npos);
    }
}
#endif // GGML_USE_METAL
