#include "GgmlRuntimePaths.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <span>

namespace GgmlRuntimePaths {

namespace {

bool ends_with(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

bool has_windows_runtime_payload(const std::filesystem::path& dir,
                                 std::span<const char* const> required_files)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    for (const char* filename : required_files) {
        if (!std::filesystem::exists(dir / filename, ec)) {
            return false;
        }
    }

    return true;
}

bool has_regular_entry(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) &&
           (std::filesystem::is_regular_file(path, ec) || std::filesystem::is_symlink(path, ec));
}

std::string lower_copy(std::string_view value)
{
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::optional<std::string> normalized_linux_accelerator_backend_key(std::string_view backend_key)
{
    std::string normalized = lower_copy(backend_key);
    if (normalized == "cuda" || normalized == "vulkan") {
        return normalized;
    }
    return std::nullopt;
}

std::optional<std::string> detect_linux_accelerator_backend_key(const std::filesystem::path& dir)
{
    if (has_regular_entry(dir / "libggml-cuda.so")) {
        return std::string("cuda");
    }
    if (has_regular_entry(dir / "libggml-vulkan.so")) {
        return std::string("vulkan");
    }
    return std::nullopt;
}

bool has_linux_runtime_dependencies(const std::filesystem::path& dir)
{
    constexpr std::array versioned_core_files = {
        "libllama.so.0",
        "libggml.so.0",
        "libggml-base.so.0",
        "libmtmd.so.0",
    };

    const bool has_core = std::all_of(
        versioned_core_files.begin(), versioned_core_files.end(), [&](const char* filename) {
        return has_regular_entry(dir / filename);
    });
    return has_core && has_regular_entry(dir / "libggml-cpu.so");
}

const char* linux_accelerator_plugin_name(std::string_view backend_key)
{
    if (backend_key == "cuda") {
        return "libggml-cuda.so";
    }
    if (backend_key == "vulkan") {
        return "libggml-vulkan.so";
    }
    return nullptr;
}

void set_env_value(const char* key, const char* value)
{
#ifdef _WIN32
    _putenv_s(key, value ? value : "");
#else
    if (value && value[0] != '\0') {
        setenv(key, value, 1);
    } else {
        unsetenv(key);
    }
#endif
}

} // namespace

bool has_payload(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(
             dir,
             std::filesystem::directory_options::skip_permission_denied,
             ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename.rfind("libggml-", 0) != 0) {
            continue;
        }
        if (ends_with(filename, ".so") || ends_with(filename, ".dylib")) {
            return true;
        }
    }

    return false;
}

std::vector<std::filesystem::path> windows_cpu_runtime_candidate_dirs(
    const std::filesystem::path& exe_path)
{
    if (exe_path.empty()) {
        return {};
    }

    const std::filesystem::path exe_dir = exe_path.parent_path();
    return {
        exe_dir / "lib" / "ggml" / "wocuda",
        exe_dir / "ggml" / "wocuda",
        exe_dir / "lib" / "ggml" / "wvulkan",
        exe_dir / "ggml" / "wvulkan",
    };
}

std::optional<std::filesystem::path> resolve_windows_cpu_runtime_dir(
    const std::filesystem::path& exe_path)
{
    constexpr std::array required_files = {
        "llama.dll",
        "ggml.dll",
        "ggml-cpu.dll",
    };

    for (const auto& candidate : windows_cpu_runtime_candidate_dirs(exe_path)) {
        if (has_windows_runtime_payload(candidate, required_files)) {
            return candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> windows_vulkan_payload_candidate_dirs(
    const std::filesystem::path& exe_path)
{
    if (exe_path.empty()) {
        return {};
    }

    const std::filesystem::path exe_dir = exe_path.parent_path();
    return {
        exe_dir / "lib" / "precompiled" / "vulkan-blas" / "bin",
        exe_dir / "lib" / "precompiled" / "vulkan" / "bin",
    };
}

std::optional<std::filesystem::path> resolve_windows_vulkan_payload_dir(
    const std::filesystem::path& exe_path)
{
    constexpr std::array required_files = {
        "llama.dll",
        "ggml.dll",
        "ggml-vulkan.dll",
        "vulkan-1.dll",
    };

    for (const auto& candidate : windows_vulkan_payload_candidate_dirs(exe_path)) {
        if (has_windows_runtime_payload(candidate, required_files)) {
            return candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> macos_candidate_dirs(
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir) {
    if (exe_path.empty()) {
        return {};
    }

    const std::filesystem::path exe_dir = exe_path.parent_path();
    const std::filesystem::path subdir(ggml_subdir);

    return {
        exe_dir / "../lib" / "precompiled-m1",
        exe_dir / "../lib" / "precompiled-m2",
        exe_dir / "../lib" / "precompiled-intel",
        exe_dir / "../lib" / subdir,
        exe_dir / "../../lib" / subdir,
        exe_dir / "../lib" / "aifilesorter",
        exe_dir / "../../lib" / "aifilesorter",
        exe_dir / "../lib",
        exe_dir / "../../lib",
    };
}

std::optional<std::filesystem::path> resolve_macos_backend_dir(
    const std::optional<std::filesystem::path>& current_dir,
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir) {
    if (current_dir && has_payload(*current_dir)) {
        return current_dir->lexically_normal();
    }

    for (const auto& candidate : macos_candidate_dirs(exe_path, ggml_subdir)) {
        if (has_payload(candidate)) {
            return candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

LinuxAcceleratorPayloadCheck validate_linux_accelerator_payload(
    const std::filesystem::path& dir,
    std::string_view backend_key)
{
    LinuxAcceleratorPayloadCheck result;
    const auto normalized_backend = normalized_linux_accelerator_backend_key(backend_key);
    if (!normalized_backend) {
        result.reason = "unsupported accelerator backend key";
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        result.reason = "runtime directory does not exist";
        return result;
    }

    const char* plugin_name = linux_accelerator_plugin_name(*normalized_backend);
    if (!plugin_name) {
        result.reason = "unsupported accelerator backend key";
        return result;
    }
    if (!has_regular_entry(dir / plugin_name)) {
        result.reason = std::string("missing required backend plugin '") + plugin_name + "'";
        return result;
    }

    if (!has_linux_runtime_dependencies(dir)) {
        result.reason =
            "missing Linux runtime dependencies required by the ggml backend payload "
            "(libllama.so.0, libggml.so.0, libggml-base.so.0, libmtmd.so.0, libggml-cpu.so)";
        return result;
    }

    result.valid = true;
    return result;
}

std::optional<std::string> sanitize_linux_backend_environment()
{
#if !defined(__linux__)
    return std::nullopt;
#else
    const char* ggml_dir_value = std::getenv("AI_FILE_SORTER_GGML_DIR");
    if (!ggml_dir_value || ggml_dir_value[0] == '\0') {
        return std::nullopt;
    }

    std::optional<std::string> backend_key;
    if (const char* backend_env = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
        backend_env && backend_env[0] != '\0') {
        backend_key = normalized_linux_accelerator_backend_key(backend_env);
    }
    if (!backend_key) {
        if (const char* device_env = std::getenv("LLAMA_ARG_DEVICE");
            device_env && device_env[0] != '\0') {
            backend_key = normalized_linux_accelerator_backend_key(device_env);
        }
    }

    const std::filesystem::path ggml_dir = std::filesystem::path(ggml_dir_value);
    if (!backend_key) {
        backend_key = detect_linux_accelerator_backend_key(ggml_dir);
    }
    if (!backend_key) {
        return std::nullopt;
    }

    const auto validation = validate_linux_accelerator_payload(ggml_dir, *backend_key);
    if (validation.valid) {
        return std::nullopt;
    }

    set_env_value("AI_FILE_SORTER_GPU_BACKEND", "cpu");
    set_env_value("GGML_DISABLE_CUDA", "1");
    set_env_value("LLAMA_ARG_DEVICE", nullptr);
    set_env_value("AI_FILE_SORTER_GGML_DIR", nullptr);

    return "Rejected stale " + *backend_key + " runtime payload '" + ggml_dir.string() +
           "': " + validation.reason + ". Falling back to CPU.";
#endif
}

} // namespace GgmlRuntimePaths
