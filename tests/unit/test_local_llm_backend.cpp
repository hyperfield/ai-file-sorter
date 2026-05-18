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

TEST_CASE("Image categorization uses an image-specific system prompt") {
    const std::string prompt_path =
        "home/theuser/Downloads/django_project_dashboard.png\n"
        "Image description: Django administration interface screenshot showing a project dashboard.";
    const std::string prompt = LocalLLMTestAccess::categorization_system_prompt_for_testing(
        prompt_path,
        FileType::File);

    CHECK(prompt.find("always use Images as the main category") != std::string::npos);
    CHECK(prompt.find("Allowed main categories") != std::string::npos);
    CHECK(prompt.find("Reply with exactly one line in the format Images : <Subcategory>") != std::string::npos);
    CHECK(prompt.find("Dashboards, Forms, Product Pages, Interfaces, File Managers") != std::string::npos);
    CHECK(prompt.find("Use any provided image description as the primary evidence") != std::string::npos);
    CHECK(prompt.find("If the file is an installer") == std::string::npos);
    CHECK(prompt.find("If uncertain, make your best guess from the name only") == std::string::npos);
}

TEST_CASE("Generic file categorization keeps the general system prompt") {
    const std::string prompt = LocalLLMTestAccess::categorization_system_prompt_for_testing(
        "home/theuser/Downloads/freetube_0.23.5_amd64.deb",
        FileType::File);

    CHECK(prompt.find("Allowed main categories") != std::string::npos);
    CHECK(prompt.find("If the file is an installer") != std::string::npos);
    CHECK(prompt.find("If uncertain, make your best guess from the name only") != std::string::npos);
}

TEST_CASE("Document categorization uses a document-specific system prompt") {
    const auto check_document_prompt = [](const std::string& prompt_path) {
        const std::string prompt = LocalLLMTestAccess::categorization_system_prompt_for_testing(
            prompt_path,
            FileType::File);

        CHECK(prompt.find("filesystem organization") != std::string::npos);
        CHECK(prompt.find("Allowed main categories") != std::string::npos);
        CHECK(prompt.find("keep the main category broad and filesystem-friendly") != std::string::npos);
        CHECK(prompt.find("If the file is an installer") == std::string::npos);
        CHECK(prompt.find("always use Images as the main category") == std::string::npos);
    };

    check_document_prompt("home/theuser/Downloads/pci_dss_quick_reference.pdf");
    check_document_prompt("home/theuser/Downloads/file-sample_100kB.doc");
}

TEST_CASE("Image categorization uses a concise user prompt without guidance block") {
    const std::string file_name = "samsung_qn90c_tv.jpeg";
    const std::string file_path =
        "home/theuser/Downloads/samsung_qn90c_tv.jpeg\n"
        "Image description: Product page screenshot showing Samsung QN90C televisions arranged in a grid, with specifications and prices.";
    const std::string consistency_context =
        "Image categorization guidance:\n"
        "- Categorize the subject matter shown in the image, not merely the file format.\n"
        "- For UI-like screenshots, prefer labels that describe what is on screen.\n\n"
        "Allowed main categories (pick exactly one label from the numbered list):\n"
        "1) Electronics\n"
        "2) Images";

    const std::string prompt = LocalLLMTestAccess::categorization_user_prompt_for_testing(
        file_name,
        file_path,
        FileType::File,
        consistency_context);

    CHECK(prompt.find("Categorize this image file for file organization.") == 0);
    CHECK(prompt.find("File name: samsung_qn90c_tv.jpeg") != std::string::npos);
    CHECK(prompt.find("Path: home/theuser/Downloads/samsung_qn90c_tv.jpeg") != std::string::npos);
    CHECK(prompt.find("Image description: Product page screenshot showing Samsung QN90C televisions arranged in a grid, with specifications and prices.") != std::string::npos);
    CHECK(prompt.find("Answer with exactly one line:\nImages : <Subcategory>") != std::string::npos);
    CHECK(prompt.find("Image categorization guidance:") == std::string::npos);
    CHECK(prompt.find("Allowed main categories") != std::string::npos);
    CHECK(prompt.find("Full path:") == std::string::npos);
}

TEST_CASE("Document categorization uses a document-specific user prompt") {
    const std::string file_name = "pci_dss_quick_reference.pdf";
    const std::string file_path =
        "home/theuser/Downloads/pci_dss_quick_reference.pdf\n"
        "Document summary: Quick reference to PCI DSS controls for merchants and service providers.";
    const std::string consistency_context =
        "Document categorization guidance:\n"
        "- Keep the main category stable and filesystem-oriented.\n"
        "- Put the specific topic in the subcategory.\n\n"
        "Allowed main categories (pick exactly one label from the numbered list):\n"
        "1) Documents\n"
        "2) Presentations";

    const std::string prompt = LocalLLMTestAccess::categorization_user_prompt_for_testing(
        file_name,
        file_path,
        FileType::File,
        consistency_context);

    CHECK(prompt.find("Categorize this document file for file organization.") == 0);
    CHECK(prompt.find("File name: pci_dss_quick_reference.pdf") != std::string::npos);
    CHECK(prompt.find("Path: home/theuser/Downloads/pci_dss_quick_reference.pdf") != std::string::npos);
    CHECK(prompt.find("Document summary: Quick reference to PCI DSS controls for merchants and service providers.") != std::string::npos);
    CHECK(prompt.find("Document categorization guidance:") != std::string::npos);
    CHECK(prompt.find("Allowed main categories") != std::string::npos);
    CHECK(prompt.find("Answer with exactly one line:\n<Main category> : <Subcategory>") != std::string::npos);
    CHECK(prompt.find("Full path:") == std::string::npos);

    const std::string legacy_file_name = "file-sample_100kB.doc";
    const std::string legacy_file_path = "home/theuser/Downloads/file-sample_100kB.doc";
    const std::string legacy_prompt = LocalLLMTestAccess::categorization_user_prompt_for_testing(
        legacy_file_name,
        legacy_file_path,
        FileType::File,
        consistency_context);

    CHECK(legacy_prompt.find("Categorize this document file for file organization.") == 0);
    CHECK(legacy_prompt.find("File name: file-sample_100kB.doc") != std::string::npos);
    CHECK(legacy_prompt.find("Path: home/theuser/Downloads/file-sample_100kB.doc") != std::string::npos);
    CHECK(legacy_prompt.find("Document summary:") == std::string::npos);
    CHECK(legacy_prompt.find("Allowed main categories") != std::string::npos);
    CHECK(legacy_prompt.find("Full path:") == std::string::npos);
}

TEST_CASE("Generic file categorization user prompt includes explicit answer format") {
    const std::string prompt = LocalLLMTestAccess::categorization_user_prompt_for_testing(
        "Git-2.50.0.2-64-bit.exe",
        "home/theuser/Downloads/Git-2.50.0.2-64-bit.exe",
        FileType::File,
        "Allowed main categories (pick exactly one label from the numbered list):\n"
        "1) Software\n"
        "2) Installers\n"
        "3) Drivers\n"
        "4) Operating Systems\n"
        "5) Other");

    CHECK(prompt.find("Categorize this file.") == 0);
    CHECK(prompt.find("Full path: home/theuser/Downloads/Git-2.50.0.2-64-bit.exe") != std::string::npos);
    CHECK(prompt.find("File name: Git-2.50.0.2-64-bit.exe") != std::string::npos);
    CHECK(prompt.find("Allowed main categories") != std::string::npos);
    CHECK(prompt.find("Answer with exactly one line:\n<Main category> : <Subcategory>") != std::string::npos);
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
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) { return true; });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
}

TEST_CASE("LocalLLMClient builds a descending GPU-layer retry ladder from optimistic and conservative estimates") {
    const auto candidates =
        LocalLLMTestAccess::gpu_layer_retry_candidates_for_testing(20, 15);

    REQUIRE(candidates == std::vector<int>{20, 15, 11, 8, 6, 4, 3, 2, 1});
}

TEST_CASE("LocalLLMClient deduplicates matching retry estimates before reducing GPU layers") {
    const auto candidates =
        LocalLLMTestAccess::gpu_layer_retry_candidates_for_testing(15, 15);

    REQUIRE(candidates == std::vector<int>{15, 11, 8, 6, 4, 3, 2, 1});
}

TEST_CASE("CUDA override is applied when backend is available") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "7");
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view backend_name) {
        return backend_name == "CUDA";
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        return std::nullopt;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 7);
}

TEST_CASE("CUDA backend reports low GPU memory before load") {
    TempModelFile model(48, 64 * 1024 * 1024);
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view backend_name) {
        return backend_name == "CUDA";
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        TestHooks::BackendMemoryInfo info;
        info.memory.free_bytes = 1ULL * 1024ULL * 1024ULL;
        info.memory.total_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
        return info;
    });

    const auto result = LocalLLMTestAccess::prepare_model_params_result_for_testing(
        model.path().string());
    REQUIRE(result.params.n_gpu_layers == 0);
    REQUIRE(result.status == LocalLLMClient::Status::GpuLowMemoryFallbackToCpu);
}

TEST_CASE("CUDA backend falls back to CPU when backend memory metrics are unavailable") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view backend_name) {
        return backend_name == "CUDA";
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        return std::nullopt;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
}

TEST_CASE("Auto backend prefers CUDA when both backends are possible") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", std::nullopt);
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "7");
    BackendProbeGuard backend_guard;
    TestHooks::set_backend_availability_probe([](std::string_view backend_name) {
        return backend_name == "CUDA";
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        return std::nullopt;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 7);
}

TEST_CASE("Auto backend falls back to Vulkan when CUDA is disabled") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", std::nullopt);
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", "1");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "12");
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 12);
}

TEST_CASE("CUDA fallback when no GPU is available") {
    TempModelFile model;
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "cuda");
    EnvVarGuard disable_cuda("GGML_DISABLE_CUDA", std::nullopt);
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) { return false; });

    auto params = LocalLLMTestAccess::prepare_model_params_for_testing(
        model.path().string());
    REQUIRE(params.n_gpu_layers == 0);
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

TEST_CASE("Vulkan backend reports low GPU memory before load") {
    TempModelFile model(48, 64 * 1024 * 1024);
    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
    EnvVarGuard llama_device("LLAMA_ARG_DEVICE", std::nullopt);
    BackendProbeGuard guard;
    TestHooks::set_backend_availability_probe([](std::string_view) {
        return true;
    });
    TestHooks::set_backend_memory_probe([](std::string_view) {
        TestHooks::BackendMemoryInfo info;
        info.memory.free_bytes = 1ULL * 1024ULL * 1024ULL;
        info.memory.total_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
        return info;
    });

    const auto result = LocalLLMTestAccess::prepare_model_params_result_for_testing(
        model.path().string());
    REQUIRE(result.params.n_gpu_layers == 0);
    REQUIRE(result.status == LocalLLMClient::Status::GpuLowMemoryFallbackToCpu);
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
