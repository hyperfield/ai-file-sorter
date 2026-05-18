#include <catch2/catch_test_macros.hpp>

#include "GgmlRuntimePaths.hpp"
#include "TestHelpers.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>

TEST_CASE("macOS ggml runtime candidates stay relative to the app layout") {
    const std::filesystem::path exe =
        "/tmp/AIFileSorter.app/Contents/MacOS/aifilesorter";

    const auto candidates =
        GgmlRuntimePaths::macos_candidate_dirs(exe, "precompiled-m2");

    REQUIRE_FALSE(candidates.empty());
    REQUIRE(candidates[0] ==
            std::filesystem::path("/tmp/AIFileSorter.app/Contents/MacOS/../lib/precompiled-m1"));
    REQUIRE(candidates[1] ==
            std::filesystem::path("/tmp/AIFileSorter.app/Contents/MacOS/../lib/precompiled-m2"));
    REQUIRE(candidates[5] ==
            std::filesystem::path("/tmp/AIFileSorter.app/Contents/MacOS/../lib/aifilesorter"));
    REQUIRE(candidates[6] ==
            std::filesystem::path("/tmp/AIFileSorter.app/Contents/MacOS/../../lib/aifilesorter"));
    REQUIRE(std::find(candidates.begin(),
                      candidates.end(),
                      std::filesystem::path("/usr/local/lib")) == candidates.end());
    REQUIRE(std::find(candidates.begin(),
                      candidates.end(),
                      std::filesystem::path("/opt/homebrew/lib")) == candidates.end());
}

TEST_CASE("Windows Vulkan payload candidates prefer the BLAS runtime layout") {
    const std::filesystem::path exe = R"(C:\AIFileSorter\aifilesorter.exe)";

    const auto candidates = GgmlRuntimePaths::windows_vulkan_payload_candidate_dirs(exe);

    REQUIRE(candidates.size() == 2);
    REQUIRE(candidates[0] ==
            std::filesystem::path(R"(C:\AIFileSorter\lib\precompiled\vulkan-blas\bin)"));
    REQUIRE(candidates[1] ==
            std::filesystem::path(R"(C:\AIFileSorter\lib\precompiled\vulkan\bin)"));
}

TEST_CASE("Windows CPU runtime candidates fall back to the Vulkan runtime layout") {
    const std::filesystem::path exe = R"(C:\AIFileSorter\aifilesorter.exe)";

    const auto candidates = GgmlRuntimePaths::windows_cpu_runtime_candidate_dirs(exe);

    REQUIRE(candidates.size() == 4);
    REQUIRE(candidates[0] ==
            std::filesystem::path(R"(C:\AIFileSorter\lib\ggml\wocuda)"));
    REQUIRE(candidates[1] ==
            std::filesystem::path(R"(C:\AIFileSorter\ggml\wocuda)"));
    REQUIRE(candidates[2] ==
            std::filesystem::path(R"(C:\AIFileSorter\lib\ggml\wvulkan)"));
    REQUIRE(candidates[3] ==
            std::filesystem::path(R"(C:\AIFileSorter\ggml\wvulkan)"));
}

TEST_CASE("Windows CPU runtime resolution falls back to the Vulkan runtime layout") {
    TempDir temp_dir;
    const auto root = temp_dir.path();
    const auto exe = root / "aifilesorter.exe";
    const auto fallback = root / "lib" / "ggml" / "wvulkan";

    std::ofstream(exe).put('x');

    std::filesystem::create_directories(fallback);
    std::ofstream(fallback / "llama.dll").put('x');
    std::ofstream(fallback / "ggml.dll").put('x');
    std::ofstream(fallback / "ggml-cpu.dll").put('x');

    const auto resolved = GgmlRuntimePaths::resolve_windows_cpu_runtime_dir(exe);

    REQUIRE(resolved.has_value());
    REQUIRE(*resolved == fallback);
}

TEST_CASE("Windows Vulkan payload resolution prefers the BLAS runtime layout") {
    TempDir temp_dir;
    const auto root = temp_dir.path();
    const auto exe = root / "aifilesorter.exe";
    const auto preferred = root / "lib" / "precompiled" / "vulkan-blas" / "bin";
    const auto fallback = root / "lib" / "precompiled" / "vulkan" / "bin";

    std::ofstream(exe).put('x');

    std::filesystem::create_directories(preferred);
    std::filesystem::create_directories(fallback);
    for (const auto& dir : {preferred, fallback}) {
        std::ofstream(dir / "llama.dll").put('x');
        std::ofstream(dir / "ggml.dll").put('x');
        std::ofstream(dir / "ggml-vulkan.dll").put('x');
        std::ofstream(dir / "vulkan-1.dll").put('x');
    }

    const auto resolved = GgmlRuntimePaths::resolve_windows_vulkan_payload_dir(exe);

    REQUIRE(resolved.has_value());
    REQUIRE(*resolved == preferred);
}

TEST_CASE("Linux Vulkan payload validation rejects stale runtime directories") {
    TempDir temp_dir;
    const auto payload = temp_dir.path() / "vulkan";
    std::filesystem::create_directories(payload);

    std::ofstream(payload / "libggml-vulkan.so").put('x');
    std::ofstream(payload / "libllama.so").put('x');
    std::ofstream(payload / "libggml.so").put('x');
    std::ofstream(payload / "libggml-base.so").put('x');
    std::ofstream(payload / "libmtmd.so").put('x');

    const auto validation =
        GgmlRuntimePaths::validate_linux_accelerator_payload(payload, "vulkan");

    REQUIRE_FALSE(validation.valid);
    REQUIRE(validation.reason.find("missing Linux runtime dependencies") != std::string::npos);
}

TEST_CASE("Linux Vulkan payload validation accepts compatible runtime directories") {
    TempDir temp_dir;
    const auto payload = temp_dir.path() / "vulkan";
    std::filesystem::create_directories(payload);

    std::ofstream(payload / "libggml-vulkan.so").put('x');
    std::ofstream(payload / "libllama.so.0").put('x');
    std::ofstream(payload / "libggml.so.0").put('x');
    std::ofstream(payload / "libggml-base.so.0").put('x');
    std::ofstream(payload / "libmtmd.so.0").put('x');
    std::ofstream(payload / "libggml-cpu.so").put('x');

    const auto validation =
        GgmlRuntimePaths::validate_linux_accelerator_payload(payload, "vulkan");

    REQUIRE(validation.valid);
    REQUIRE(validation.reason.empty());
}

#if defined(__linux__)
TEST_CASE("Linux backend environment sanitization demotes stale Vulkan payloads to CPU") {
    TempDir temp_dir;
    const auto payload = temp_dir.path() / "vulkan";
    std::filesystem::create_directories(payload);

    std::ofstream(payload / "libggml-vulkan.so").put('x');
    std::ofstream(payload / "libllama.so").put('x');
    std::ofstream(payload / "libggml.so").put('x');
    std::ofstream(payload / "libggml-base.so").put('x');
    std::ofstream(payload / "libmtmd.so").put('x');

    EnvVarGuard backend_guard("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard device_guard("LLAMA_ARG_DEVICE", "vulkan");
    EnvVarGuard ggml_dir_guard("AI_FILE_SORTER_GGML_DIR", payload.string());
    EnvVarGuard disable_guard("GGML_DISABLE_CUDA", std::nullopt);

    const auto reason = GgmlRuntimePaths::sanitize_linux_backend_environment();

    REQUIRE(reason.has_value());
    REQUIRE(reason->find("Rejected stale vulkan runtime payload") != std::string::npos);
    REQUIRE(std::string(std::getenv("AI_FILE_SORTER_GPU_BACKEND")) == "cpu");
    REQUIRE(std::string(std::getenv("GGML_DISABLE_CUDA")) == "1");
    REQUIRE(std::getenv("AI_FILE_SORTER_GGML_DIR") == nullptr);
    REQUIRE(std::getenv("LLAMA_ARG_DEVICE") == nullptr);
}
#endif

TEST_CASE("macOS ggml runtime resolution prefers bundled directories over generic siblings") {
    TempDir temp_dir;
    const auto root = temp_dir.path();
    const auto exe = root / "bin" / "m2" / "aifilesorter";
    const auto bundled = root / "lib" / "precompiled-m2";
    const auto generic = root / "lib" / "aifilesorter";

    std::filesystem::create_directories(exe.parent_path());
    std::ofstream(exe).put('x');

    std::filesystem::create_directories(bundled);
    std::ofstream(bundled / "libggml-metal.dylib").put('x');

    std::filesystem::create_directories(generic);
    std::ofstream(generic / "libggml-cpu.dylib").put('x');

    const auto resolved = GgmlRuntimePaths::resolve_macos_backend_dir(
        std::nullopt,
        exe,
        "precompiled-m2");

    REQUIRE(resolved.has_value());
    REQUIRE(*resolved == bundled);
}

TEST_CASE("macOS ggml runtime resolution preserves a valid explicit override") {
    TempDir temp_dir;
    const auto root = temp_dir.path();
    const auto exe = root / "bin" / "aifilesorter";
    const auto custom = root / "custom-ggml";

    std::filesystem::create_directories(exe.parent_path());
    std::ofstream(exe).put('x');

    std::filesystem::create_directories(custom);
    std::ofstream(custom / "libggml-blas.dylib").put('x');

    const auto resolved = GgmlRuntimePaths::resolve_macos_backend_dir(
        custom,
        exe,
        "precompiled");

    REQUIRE(resolved.has_value());
    REQUIRE(*resolved == custom);
}
