#include "WindowsCudaProbe.hpp"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <system_error>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace WindowsCudaProbe {

#ifdef _WIN32
namespace {

using DriverInitFunc = int (__stdcall *)(unsigned int);
using DriverGetCountFunc = int (__stdcall *)(int *);
using DriverGetVersionFunc = int (__stdcall *)(int *);
using RuntimeGetCountFunc = int (*)(int *);
using RuntimeSetDeviceFunc = int (*)(int);
using RuntimeMemGetInfoFunc = int (*)(size_t *, size_t *);

struct LibraryHandle {
    HMODULE value{nullptr};

    LibraryHandle() = default;
    explicit LibraryHandle(HMODULE handle) : value(handle) {}
    LibraryHandle(const LibraryHandle&) = delete;
    LibraryHandle& operator=(const LibraryHandle&) = delete;

    LibraryHandle(LibraryHandle&& other) noexcept : value(other.value) {
        other.value = nullptr;
    }

    LibraryHandle& operator=(LibraryHandle&& other) noexcept {
        if (this != &other) {
            reset();
            value = other.value;
            other.value = nullptr;
        }
        return *this;
    }

    ~LibraryHandle() {
        reset();
    }

    void reset(HMODULE handle = nullptr) {
        if (value) {
            FreeLibrary(value);
        }
        value = handle;
    }

    [[nodiscard]] bool valid() const {
        return value != nullptr;
    }
};

std::wstring to_lower_copy(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring normalize_key(const std::filesystem::path& path)
{
    return to_lower_copy(path.lexically_normal().wstring());
}

void append_unique(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    if (candidate.empty()) {
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(candidate, ec)) {
        return;
    }

    const std::filesystem::path normalized = candidate.lexically_normal();
    const std::wstring key = normalize_key(normalized);
    const auto duplicate = std::find_if(paths.begin(), paths.end(), [&](const std::filesystem::path& existing) {
        return normalize_key(existing) == key;
    });
    if (duplicate == paths.end()) {
        paths.push_back(normalized);
    }
}

std::optional<std::wstring> read_env_var(const wchar_t* name)
{
    const wchar_t* value = _wgetenv(name);
    if (!value || value[0] == L'\0') {
        return std::nullopt;
    }
    return std::wstring(value);
}

void add_cuda_root(std::vector<std::filesystem::path>& directories, const std::filesystem::path& root)
{
    if (root.empty()) {
        return;
    }

    const std::filesystem::path bin_x64_dir = root / L"bin" / L"x64";
    const std::filesystem::path bin_dir = root / L"bin";
    std::error_code ec;
    if (std::filesystem::exists(bin_x64_dir, ec)) {
        append_unique(directories, bin_x64_dir);
    }
    ec.clear();
    if (std::filesystem::exists(bin_dir, ec)) {
        append_unique(directories, bin_dir);
    }

    append_unique(directories, root);
}

std::vector<std::filesystem::path> path_entries()
{
    std::vector<std::filesystem::path> entries;
    const auto path_env = read_env_var(L"PATH");
    if (!path_env.has_value()) {
        return entries;
    }

    size_t start = 0;
    while (start <= path_env->size()) {
        const size_t end = path_env->find(L';', start);
        const std::wstring token = path_env->substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!token.empty()) {
            append_unique(entries, std::filesystem::path(token));
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }

    return entries;
}

void add_cuda_env_roots(std::vector<std::filesystem::path>& directories)
{
    if (const auto cuda_path = read_env_var(L"CUDA_PATH")) {
        add_cuda_root(directories, std::filesystem::path(*cuda_path));
    }

    LPWCH environment_block = GetEnvironmentStringsW();
    if (!environment_block) {
        return;
    }

    const wchar_t* current = environment_block;
    while (*current != L'\0') {
        std::wstring entry(current);
        const size_t separator = entry.find(L'=');
        if (separator != std::wstring::npos) {
            std::wstring name = entry.substr(0, separator);
            if (name.rfind(L"CUDA_PATH_V", 0) == 0) {
                add_cuda_root(directories, std::filesystem::path(entry.substr(separator + 1)));
            }
        }
        current += entry.size() + 1;
    }

    FreeEnvironmentStringsW(environment_block);
}

void add_default_cuda_roots(std::vector<std::filesystem::path>& directories)
{
    const std::filesystem::path toolkit_root = L"C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA";
    std::error_code ec;
    if (!std::filesystem::exists(toolkit_root, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(toolkit_root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory()) {
            continue;
        }
        add_cuda_root(directories, entry.path());
    }
}

std::vector<std::filesystem::path> candidate_runtime_directories()
{
    std::vector<std::filesystem::path> directories;
    add_cuda_env_roots(directories);
    add_default_cuda_roots(directories);

    for (const auto& entry : path_entries()) {
        append_unique(directories, entry);
    }

    return directories;
}

int parse_runtime_version_token(const std::filesystem::path& path)
{
    const std::wstring name = to_lower_copy(path.filename().wstring());
    constexpr std::wstring_view prefix = L"cudart64_";
    constexpr std::wstring_view suffix = L".dll";
    if (!name.starts_with(prefix) || !name.ends_with(suffix) || name.size() <= prefix.size() + suffix.size()) {
        return 0;
    }

    const std::wstring token = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    if (token.empty() || !std::all_of(token.begin(), token.end(), [](wchar_t ch) { return std::iswdigit(ch) != 0; })) {
        return 0;
    }

    try {
        return std::stoi(token);
    } catch (...) {
        return 0;
    }
}

struct RuntimeCandidate {
    std::filesystem::path path;
    int version_token{0};
};

std::vector<RuntimeCandidate> candidate_runtime_libraries()
{
    std::vector<RuntimeCandidate> candidates;

    for (const auto& directory : candidate_runtime_directories()) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_regular_file()) {
                continue;
            }

            const int version_token = parse_runtime_version_token(entry.path());
            if (version_token <= 0) {
                continue;
            }

            const std::wstring key = normalize_key(entry.path());
            const auto duplicate = std::find_if(candidates.begin(), candidates.end(), [&](const RuntimeCandidate& candidate) {
                return normalize_key(candidate.path) == key;
            });
            if (duplicate == candidates.end()) {
                candidates.push_back(RuntimeCandidate{entry.path().lexically_normal(), version_token});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const RuntimeCandidate& lhs, const RuntimeCandidate& rhs) {
        if (lhs.version_token != rhs.version_token) {
            return lhs.version_token > rhs.version_token;
        }

        const std::wstring lhs_key = normalize_key(lhs.path.parent_path());
        const std::wstring rhs_key = normalize_key(rhs.path.parent_path());
        const bool lhs_is_x64 = lhs_key.find(L"\\bin\\x64") != std::wstring::npos;
        const bool rhs_is_x64 = rhs_key.find(L"\\bin\\x64") != std::wstring::npos;
        if (lhs_is_x64 != rhs_is_x64) {
            return lhs_is_x64;
        }

        return normalize_key(lhs.path) < normalize_key(rhs.path);
    });

    return candidates;
}

LibraryHandle load_library_from_path(const std::filesystem::path& path)
{
    return LibraryHandle(LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
}

bool resolve_runtime_symbols(const LibraryHandle& library,
                            RuntimeGetCountFunc& get_count,
                            RuntimeSetDeviceFunc& set_device,
                            RuntimeMemGetInfoFunc& mem_get_info)
{
    get_count = reinterpret_cast<RuntimeGetCountFunc>(GetProcAddress(library.value, "cudaGetDeviceCount"));
    set_device = reinterpret_cast<RuntimeSetDeviceFunc>(GetProcAddress(library.value, "cudaSetDevice"));
    mem_get_info = reinterpret_cast<RuntimeMemGetInfoFunc>(GetProcAddress(library.value, "cudaMemGetInfo"));
    return get_count && set_device && mem_get_info;
}

bool runtime_has_usable_device(const std::filesystem::path& runtime_path, int* device_count = nullptr)
{
    LibraryHandle runtime = load_library_from_path(runtime_path);
    if (!runtime.valid()) {
        return false;
    }

    RuntimeGetCountFunc get_count = nullptr;
    RuntimeSetDeviceFunc set_device = nullptr;
    RuntimeMemGetInfoFunc mem_get_info = nullptr;
    if (!resolve_runtime_symbols(runtime, get_count, set_device, mem_get_info)) {
        return false;
    }

    int count = 0;
    if (get_count(&count) != 0 || count <= 0) {
        return false;
    }
    if (device_count) {
        *device_count = count;
    }

    if (set_device(0) != 0) {
        return false;
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    return mem_get_info(&free_bytes, &total_bytes) == 0;
}

struct DllDirectoryCookie {
    DLL_DIRECTORY_COOKIE cookie{nullptr};

    DllDirectoryCookie() = default;
    explicit DllDirectoryCookie(DLL_DIRECTORY_COOKIE value) : cookie(value) {}
    DllDirectoryCookie(const DllDirectoryCookie&) = delete;
    DllDirectoryCookie& operator=(const DllDirectoryCookie&) = delete;

    DllDirectoryCookie(DllDirectoryCookie&& other) noexcept : cookie(other.cookie) {
        other.cookie = nullptr;
    }

    DllDirectoryCookie& operator=(DllDirectoryCookie&& other) noexcept {
        if (this != &other) {
            reset();
            cookie = other.cookie;
            other.cookie = nullptr;
        }
        return *this;
    }

    ~DllDirectoryCookie() {
        reset();
    }

    void reset(DLL_DIRECTORY_COOKIE value = nullptr) {
        if (cookie) {
            RemoveDllDirectory(cookie);
        }
        cookie = value;
    }

    [[nodiscard]] bool valid() const {
        return cookie != nullptr;
    }
};

std::vector<DllDirectoryCookie> add_user_directories(const std::vector<std::filesystem::path>& directories)
{
    std::vector<DllDirectoryCookie> cookies;
    cookies.reserve(directories.size());

    for (const auto& directory : directories) {
        std::error_code ec;
        if (directory.empty() || !std::filesystem::exists(directory, ec)) {
            continue;
        }
        if (DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(directory.c_str())) {
            cookies.emplace_back(cookie);
        }
    }

    return cookies;
}

bool can_load_cuda_backend(const std::filesystem::path& ggml_directory,
                           const std::filesystem::path& runtime_directory)
{
    if (ggml_directory.empty()) {
        return false;
    }

    const std::filesystem::path backend_path = ggml_directory / L"ggml-cuda.dll";
    std::error_code ec;
    if (!std::filesystem::exists(backend_path, ec)) {
        return false;
    }

    const auto cookies = add_user_directories({ggml_directory, runtime_directory});
    (void) cookies;

    LibraryHandle backend(LoadLibraryExW(
        backend_path.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    return backend.valid();
}

ProbeResult probe_impl(const std::optional<std::filesystem::path>& ggml_directory)
{
    ProbeResult result;

    LibraryHandle driver(LoadLibraryW(L"nvcuda.dll"));
    result.driver_present = driver.valid();
    if (driver.valid()) {
        auto driver_init = reinterpret_cast<DriverInitFunc>(GetProcAddress(driver.value, "cuInit"));
        auto driver_get_count = reinterpret_cast<DriverGetCountFunc>(GetProcAddress(driver.value, "cuDeviceGetCount"));
        auto driver_get_version = reinterpret_cast<DriverGetVersionFunc>(GetProcAddress(driver.value, "cuDriverGetVersion"));

        if (driver_get_version) {
            int version = 0;
            if (driver_get_version(&version) == 0) {
                result.driver_version = version;
            }
        }

        if (driver_init && driver_get_count && driver_init(0) == 0) {
            result.driver_initialized = true;
            int count = 0;
            if (driver_get_count(&count) == 0 && count > 0) {
                result.device_count = count;
            }
        }
    }

    const auto candidates = candidate_runtime_libraries();
    if (candidates.empty()) {
        result.failure_reason = "no_cuda_runtime_found";
        return result;
    }

    for (const auto& candidate : candidates) {
        result.runtime_present = true;

        int device_count = 0;
        if (!runtime_has_usable_device(candidate.path, &device_count)) {
            continue;
        }

        result.runtime_library_path = candidate.path;
        result.runtime_version_token = candidate.version_token;
        result.runtime_usable = true;
        if (device_count > 0) {
            result.device_count = device_count;
        }

        if (!ggml_directory.has_value() || ggml_directory->empty()) {
            result.backend_loadable = true;
            return result;
        }

        if (can_load_cuda_backend(*ggml_directory, candidate.path.parent_path())) {
            result.backend_loadable = true;
            return result;
        }
    }

    if (!result.runtime_usable) {
        result.failure_reason = "no_usable_cuda_runtime";
    } else if (ggml_directory.has_value() && !ggml_directory->empty()) {
        result.failure_reason = "cuda_backend_dependency_mismatch";
    }
    return result;
}

} // namespace
#endif

ProbeResult probe(const std::optional<std::filesystem::path>& ggml_directory)
{
#ifdef _WIN32
    return probe_impl(ggml_directory);
#else
    (void) ggml_directory;
    return ProbeResult{};
#endif
}

std::optional<std::filesystem::path> best_runtime_library_path()
{
    const ProbeResult result = probe(std::nullopt);
    if (!result.runtime_usable || result.runtime_library_path.empty()) {
        return std::nullopt;
    }
    return result.runtime_library_path;
}

int installed_runtime_version_token()
{
    const ProbeResult result = probe(std::nullopt);
    return result.runtime_version_token;
}

std::string best_runtime_library_name()
{
    const auto path = best_runtime_library_path();
    if (!path.has_value()) {
        return std::string();
    }
    return path->filename().string();
}

} // namespace WindowsCudaProbe
