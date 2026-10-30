#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace WindowsCudaProbe {

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
    std::string failure_reason;
};

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
} // namespace TestAccess
#endif

} // namespace WindowsCudaProbe
