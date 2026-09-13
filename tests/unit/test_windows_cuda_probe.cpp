#include <catch2/catch_test_macros.hpp>

#include "WindowsCudaProbe.hpp"

#include <filesystem>
#include <vector>

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
