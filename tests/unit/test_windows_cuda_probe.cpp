#include <catch2/catch_test_macros.hpp>

#include "WindowsCudaProbe.hpp"
#include "WindowsCudaRuntimeSelection.hpp"

#include <filesystem>
#include <vector>

namespace Selection = WindowsCudaRuntimeSelection;

TEST_CASE("WindowsCudaProbe normalizes mixed-era runtime suffixes") {
    CHECK(WindowsCudaProbe::TestAccess::runtime_version_rank("cudart64_13.dll") >
          WindowsCudaProbe::TestAccess::runtime_version_rank("cudart64_65.dll"));
    CHECK(WindowsCudaProbe::TestAccess::runtime_version_rank("cudart64_121.dll") >
          WindowsCudaProbe::TestAccess::runtime_version_rank("cudart64_110.dll"));
}

TEST_CASE("WindowsCudaProbe prefers toolkit runtimes over legacy PhysX runtimes") {
    const std::filesystem::path physx_runtime =
        R"(C:\Program Files (x86)\NVIDIA Corporation\PhysX\Common\cudart64_65.dll)";
    const std::filesystem::path toolkit_runtime =
        R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2\bin\x64\cudart64_13.dll)";

    const std::vector<std::filesystem::path> ranked =
        WindowsCudaProbe::TestAccess::rank_runtime_candidates({physx_runtime, toolkit_runtime});

    REQUIRE(ranked.size() == 2);
    CHECK(ranked.front() == toolkit_runtime);
    CHECK(ranked.back() == physx_runtime);
}

TEST_CASE("WindowsCudaProbe prefers x64 toolkit bin directories over generic toolkit copies") {
    const std::filesystem::path toolkit_root_copy =
        R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2\cudart64_13.dll)";
    const std::filesystem::path toolkit_x64_runtime =
        R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2\bin\x64\cudart64_13.dll)";

    const std::vector<std::filesystem::path> ranked =
        WindowsCudaProbe::TestAccess::rank_runtime_candidates(
            {toolkit_root_copy, toolkit_x64_runtime});

    REQUIRE(ranked.size() == 2);
    CHECK(ranked.front() == toolkit_x64_runtime);
    CHECK(ranked.back() == toolkit_root_copy);
}

TEST_CASE("WindowsCudaProbe requires the packaged CUDA backend to be loadable") {
    WindowsCudaProbe::ProbeResult result;
    result.driver_present = true;
    result.driver_initialized = true;
    result.device_count = 1;
    result.runtime_present = true;
    result.runtime_usable = true;

    SECTION("rejects an otherwise usable runtime when the backend DLL cannot load") {
        result.backend_loadable = false;
        CHECK_FALSE(WindowsCudaProbe::can_select_cuda_backend(result, true));
    }

    SECTION("rejects a missing packaged CUDA payload") {
        result.backend_loadable = true;
        CHECK_FALSE(WindowsCudaProbe::can_select_cuda_backend(result, false));
    }

    SECTION("accepts CUDA only when driver, runtime, payload, and backend load checks pass") {
        result.backend_loadable = true;
        CHECK(WindowsCudaProbe::can_select_cuda_backend(result, true));
    }
}

TEST_CASE("Windows CUDA runtime selection validates DLL names without filesystem access") {
    SECTION("parses legacy and modern CUDA suffixes") {
        struct Example {
            const char* name;
            int token;
            int rank;
        };
        const Example examples[] = {
            {"cudart64_13.dll", 13, 1300},
            {"CUDART64_65.DLL", 65, 605},
            {"cudart64_121.dll", 121, 1201},
            {"cudart64_110.dll", 110, 1100},
            {"cudart64_1200.dll", 1200, 1200},
        };
        for (const auto& example : examples) {
            CAPTURE(example.name);
            const auto candidate = Selection::candidate_for_path(example.name);
            REQUIRE(candidate.has_value());
            CHECK(candidate->version_token == example.token);
            CHECK(candidate->version_rank == example.rank);
        }
    }

    SECTION("rejects malformed names and overflowing numeric suffixes") {
        for (const char* name : {"", "cudart32_13.dll", "cudart64_.dll", "cudart64_13.dll.old",
                                 "cudart64_13x.dll", "cudart64_-13.dll", "cudart64_2147483648.dll"}) {
            CAPTURE(name);
            CHECK_FALSE(Selection::candidate_for_path(name).has_value());
        }
    }

    SECTION("ignores unusable toolkit versions and falls back to the DLL version") {
        for (const char* version : {"vunknown", "v2147483647.9"}) {
            const auto path = std::filesystem::path("C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA")
                              / version / "bin" / "x64" / "cudart64_13.dll";
            const auto candidate = Selection::candidate_for_path(path);
            REQUIRE(candidate.has_value());
            CHECK(candidate->version_rank == 1300);
        }
    }
}

TEST_CASE("Windows CUDA runtime selection handles Windows separators and case on every host") {
    const std::vector<std::filesystem::path> paths = {
        R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2\bin\x64\cudart64_13.dll)",
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.2/bin/x64/cudart64_13.dll",
        R"(C:\PROGRAM FILES/NVIDIA GPU COMPUTING TOOLKIT\CUDA/V13.2\BIN/../BIN/X64\CUDART64_13.DLL)",
        std::filesystem::path(u8"C:/Users/Ren\u00e9/NVIDIA GPU Computing Toolkit/CUDA/v13.2/bin/x64/cudart64_13.dll"),
    };
    for (const auto& path : paths) {
        CAPTURE(path);
        const auto candidate = Selection::candidate_for_path(path);
        REQUIRE(candidate.has_value());
        CHECK(candidate->version_token == 13);
        CHECK(candidate->version_rank == 1302);
        CHECK(candidate->path_kind == Selection::PathKind::ToolkitBinX64);
        CHECK(candidate->path == path.lexically_normal());
    }
}

TEST_CASE("Windows CUDA runtime selection preserves source and layout priorities") {
    SECTION("an explicit toolkit hint precedes PATH discovery") {
        const auto hint = Selection::candidate_for_path("C:/Custom CUDA/cudart64_110.dll",
                                                         Selection::DirectorySource::ToolkitHint);
        const auto path = Selection::candidate_for_path(
            R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2\bin\x64\cudart64_13.dll)");
        REQUIRE(hint.has_value());
        REQUIRE(path.has_value());
        const auto ranked = Selection::rank_candidates({*path, *hint});
        REQUIRE(ranked.size() == 2);
        CHECK(ranked.front().path == hint->path);
    }

    SECTION("preferred toolkit layouts precede newer generic and PhysX DLLs") {
        const std::vector<std::filesystem::path> preferred = {
            R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\bin\x64\cudart64_121.dll)",
            R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2\bin\cudart64_13.dll)",
            R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v14.0\cudart64_14.dll)",
            R"(C:\Runtime\cudart64_15.dll)",
            R"(C:\Program Files (x86)\NVIDIA Corporation\PhysX\Common\cudart64_15.dll)",
        };
        std::vector<std::filesystem::path> reversed(preferred.rbegin(), preferred.rend());
        CHECK(WindowsCudaProbe::TestAccess::rank_runtime_candidates(reversed) == preferred);
    }
}

TEST_CASE("Windows CUDA runtime selection orders versions and breaks ties deterministically") {
    const std::filesystem::path first = R"(C:\a\cudart64_13.dll)";
    const std::filesystem::path second = R"(C:\Z\cudart64_13.dll)";
    const std::filesystem::path older = R"(C:\b\cudart64_121.dll)";
    const auto ranked = WindowsCudaProbe::TestAccess::rank_runtime_candidates(
        {second, older, "README.txt", first});
    REQUIRE(ranked.size() == 3);
    CHECK(ranked[0] == first);
    CHECK(ranked[1] == second);
    CHECK(ranked[2] == older);
}

#ifndef _WIN32
TEST_CASE("Non-Windows CUDA probing reports no native runtime") {
    const auto result = WindowsCudaProbe::probe();
    CHECK_FALSE(result.driver_present);
    CHECK_FALSE(result.driver_initialized);
    CHECK_FALSE(result.runtime_present);
    CHECK_FALSE(result.runtime_usable);
    CHECK_FALSE(result.backend_loadable);
    CHECK(result.runtime_library_path.empty());
    CHECK_FALSE(WindowsCudaProbe::best_runtime_library_path().has_value());
    CHECK(WindowsCudaProbe::installed_runtime_version_token() == 0);
    CHECK(WindowsCudaProbe::best_runtime_library_name().empty());
}
#endif
