#include <catch2/catch_test_macros.hpp>

#include "GgmlRuntimePaths.hpp"
#include "TestHelpers.hpp"

#include <algorithm>
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
