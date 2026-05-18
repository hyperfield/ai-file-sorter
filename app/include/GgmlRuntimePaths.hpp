#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GgmlRuntimePaths {

bool has_payload(const std::filesystem::path& dir);

/**
 * @brief Result of validating a Linux accelerator runtime payload directory.
 */
struct LinuxAcceleratorPayloadCheck {
    /** @brief True when the payload is safe to use for the requested backend. */
    bool valid{false};
    /** @brief Human-readable explanation when the payload is rejected. */
    std::string reason;
};

/**
 * @brief Returns candidate packaged CPU runtime directories for Windows.
 *
 * The returned list prefers the dedicated CPU runtime layout and then falls
 * back to the packaged Vulkan runtime layout so launcher-based builds can
 * reuse `wvulkan` when it already contains the CPU backend DLLs.
 *
 * @param exe_path Path to the currently running executable.
 * @return Candidate Windows CPU runtime directories in priority order.
 */
std::vector<std::filesystem::path> windows_cpu_runtime_candidate_dirs(
    const std::filesystem::path& exe_path);

/**
 * @brief Resolves the best packaged CPU runtime directory for Windows.
 *
 * @param exe_path Path to the currently running executable.
 * @return The first existing CPU runtime directory that contains the required
 * Windows CPU backend DLLs, or `std::nullopt` when none are usable.
 */
std::optional<std::filesystem::path> resolve_windows_cpu_runtime_dir(
    const std::filesystem::path& exe_path);

/**
 * @brief Returns candidate packaged Vulkan payload directories for Windows.
 *
 * The returned list prefers the BLAS-enabled Vulkan payload layout and falls
 * back to the legacy Vulkan directory layout for compatibility.
 *
 * @param exe_path Path to the currently running executable.
 * @return Candidate Windows Vulkan payload directories in priority order.
 */
std::vector<std::filesystem::path> windows_vulkan_payload_candidate_dirs(
    const std::filesystem::path& exe_path);

/**
 * @brief Resolves the best packaged Vulkan payload directory for Windows.
 *
 * @param exe_path Path to the currently running executable.
 * @return The first existing Vulkan payload directory that contains the
 * required Windows runtime DLLs, or `std::nullopt` when none are usable.
 */
std::optional<std::filesystem::path> resolve_windows_vulkan_payload_dir(
    const std::filesystem::path& exe_path);

std::vector<std::filesystem::path> macos_candidate_dirs(
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir);

std::optional<std::filesystem::path> resolve_macos_backend_dir(
    const std::optional<std::filesystem::path>& current_dir,
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir);

/**
 * @brief Validates a staged Linux CUDA/Vulkan runtime payload before use.
 *
 * A compatible accelerator payload must contain the requested backend plugin
 * as well as the versioned core runtime aliases that `aifilesorter-bin`
 * expects at process startup.
 *
 * @param dir Candidate runtime directory.
 * @param backend_key Requested backend key, for example `cuda` or `vulkan`.
 * @return Validation outcome with a rejection reason when the payload is stale
 * or incomplete.
 */
LinuxAcceleratorPayloadCheck validate_linux_accelerator_payload(
    const std::filesystem::path& dir,
    std::string_view backend_key);

/**
 * @brief Rewrites an invalid Linux accelerator runtime environment to CPU.
 *
 * When the current environment points at a stale CUDA/Vulkan payload, this
 * helper clears the custom ggml runtime directory and forces CPU backend
 * selection so the app does not advertise or attempt to use an incompatible
 * accelerator runtime.
 *
 * @return Rejection reason when a stale accelerator payload was disabled;
 * otherwise `std::nullopt`.
 */
std::optional<std::string> sanitize_linux_backend_environment();

} // namespace GgmlRuntimePaths
