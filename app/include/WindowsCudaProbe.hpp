#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace WindowsCudaProbe {

/** @brief CUDA driver, runtime and optional backend load results for Windows. */
struct ProbeResult {
    bool driver_present{false};
    bool driver_initialized{false};
    int driver_version{0};
    int device_count{0};
    bool runtime_present{false};
    bool runtime_usable{false};
    bool backend_loadable{false};
    int runtime_version_token{0};
    std::filesystem::path runtime_library_path;
    /** @brief Win32 loader error for the preferred usable runtime, or zero on success. */
    std::uint32_t backend_load_error{0};
    std::string failure_reason;
};

/**
 * @brief Find the CUDA payload without mistaking a selected CPU/Vulkan directory for it.
 * @param executable_path Running executable; empty uses the native executable path on Windows.
 * @param preferred_directory Optional current runtime directory; accepted only with ggml-cuda.dll.
 * @return CUDA backend directory, or std::nullopt when no CUDA payload is present.
 */
std::optional<std::filesystem::path> resolve_backend_directory(
    const std::filesystem::path& executable_path,
    const std::optional<std::filesystem::path>& preferred_directory = std::nullopt);

/**
 * @brief Probe CUDA, preferring runtime DLLs bundled beside the CUDA backend.
 * @param ggml_directory Optional CUDA backend directory whose dependencies must load.
 * @return Driver/runtime status, and backend load status when a directory is supplied.
 */
ProbeResult probe(const std::optional<std::filesystem::path>& ggml_directory = std::nullopt);

/**
 * @brief Returns whether a probed CUDA stack is safe for launcher selection.
 *
 * The probe result should come from probing the packaged ggml CUDA directory
 * so `backend_loadable` reflects whether `ggml-cuda.dll` and its CUDA/cuBLAS
 * dependencies can be loaded before the main application process starts.
 *
 * @param result CUDA driver/runtime/backend probe result.
 * @param backend_payload_present True when the packaged CUDA backend payload
 * directory contains the required ggml runtime DLLs.
 * @return True only when CUDA can be selected without relying on a later
 * Windows loader failure to discover missing backend dependencies.
 */
bool can_select_cuda_backend(const ProbeResult& result, bool backend_payload_present);

std::optional<std::filesystem::path> best_runtime_library_path();
int installed_runtime_version_token();
std::string best_runtime_library_name();

#ifdef AI_FILE_SORTER_TEST_BUILD
namespace TestAccess {
int runtime_version_rank(std::string_view file_name);
std::vector<std::filesystem::path> rank_runtime_candidates(
    const std::vector<std::filesystem::path>& runtime_paths);
#ifdef _WIN32
/** @brief Discover ranked runtime paths without loading them, including a supplied bundle. */
std::vector<std::filesystem::path> runtime_candidates_for_directory(const std::filesystem::path& directory);
#endif
} // namespace TestAccess
#endif

} // namespace WindowsCudaProbe
