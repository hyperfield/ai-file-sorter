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

constexpr std::filesystem::directory_options kDirectoryIteratorOptions =
    std::filesystem::directory_options::skip_permission_denied;

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

enum class RuntimeDirectorySource {
    Packaged,
    ToolkitHint,
    Path,
};

enum class RuntimePathKind {
    ToolkitBinX64,
    ToolkitBin,
    ToolkitRoot,
    Generic,
    PhysX,
};

struct RuntimeDirectoryCandidate {
    std::filesystem::path path;
    RuntimeDirectorySource source{RuntimeDirectorySource::Path};
};

struct ParsedRuntimeToken {
    int token{0};
    int rank{0};
};

constexpr std::wstring_view kToolkitRootMarker = L"\\nvidia gpu computing toolkit\\cuda\\";
constexpr std::wstring_view kPhysXRootMarker = L"\\nvidia corporation\\physx\\common";

int normalize_runtime_version_rank(int token_value, std::size_t digit_count)
{
    if (token_value <= 0 || digit_count == 0) {
        return 0;
    }

    if (digit_count >= 3) {
        const int divisor = digit_count == 3 ? 10 : 100;
        const int major = token_value / divisor;
        const int minor = token_value % divisor;
        return major * 100 + minor;
    }

    if (digit_count == 2) {
        if (token_value >= 50) {
            const int major = token_value / 10;
            const int minor = token_value % 10;
            return major * 100 + minor;
        }
        return token_value * 100;
    }

    return token_value * 100;
}

int runtime_directory_source_priority(RuntimeDirectorySource source)
{
    switch (source) {
    case RuntimeDirectorySource::Packaged:
        return 0;
    case RuntimeDirectorySource::ToolkitHint:
        return 1;
    case RuntimeDirectorySource::Path:
        return 2;
    }
    return 3;
}

int runtime_path_kind_priority(RuntimePathKind kind)
{
    switch (kind) {
    case RuntimePathKind::ToolkitBinX64:
        return 0;
    case RuntimePathKind::ToolkitBin:
        return 1;
    case RuntimePathKind::ToolkitRoot:
        return 2;
    case RuntimePathKind::Generic:
        return 3;
    case RuntimePathKind::PhysX:
        return 4;
    }
    return 5;
}

std::optional<ParsedRuntimeToken> parse_runtime_version_token_info(const std::filesystem::path& path)
{
    const std::wstring name = to_lower_copy(path.filename().wstring());
    constexpr std::wstring_view prefix = L"cudart64_";
    constexpr std::wstring_view suffix = L".dll";
    if (!name.starts_with(prefix) || !name.ends_with(suffix) || name.size() <= prefix.size() + suffix.size()) {
        return std::nullopt;
    }

    const std::wstring token = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    if (token.empty() || !std::all_of(token.begin(), token.end(), [](wchar_t ch) { return std::iswdigit(ch) != 0; })) {
        return std::nullopt;
    }

    try {
        const int token_value = std::stoi(token);
        return ParsedRuntimeToken{token_value, normalize_runtime_version_rank(token_value, token.size())};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> parse_toolkit_version_rank(const std::filesystem::path& path)
{
    const std::wstring key = normalize_key(path);
    const size_t marker_pos = key.find(kToolkitRootMarker);
    if (marker_pos == std::wstring::npos) {
        return std::nullopt;
    }

    size_t cursor = marker_pos + kToolkitRootMarker.size();
    if (cursor >= key.size() || key[cursor] != L'v') {
        return std::nullopt;
    }
    ++cursor;

    size_t major_start = cursor;
    while (cursor < key.size() && std::iswdigit(key[cursor]) != 0) {
        ++cursor;
    }
    if (cursor == major_start) {
        return std::nullopt;
    }

    int major = 0;
    try {
        major = std::stoi(key.substr(major_start, cursor - major_start));
    } catch (...) {
        return std::nullopt;
    }

    int minor = 0;
    if (cursor < key.size() && key[cursor] == L'.') {
        ++cursor;
        size_t minor_start = cursor;
        while (cursor < key.size() && std::iswdigit(key[cursor]) != 0) {
            ++cursor;
        }
        if (cursor > minor_start) {
            try {
                minor = std::stoi(key.substr(minor_start, cursor - minor_start));
            } catch (...) {
                minor = 0;
            }
        }
    }

    return major * 100 + minor;
}

RuntimePathKind classify_runtime_path(const std::filesystem::path& path)
{
    const std::wstring key = normalize_key(path.parent_path());
    if (key.find(kPhysXRootMarker) != std::wstring::npos) {
        return RuntimePathKind::PhysX;
    }
    if (key.find(kToolkitRootMarker) != std::wstring::npos) {
        if (key.find(L"\\bin\\x64") != std::wstring::npos) {
            return RuntimePathKind::ToolkitBinX64;
        }
        if (key.find(L"\\bin") != std::wstring::npos) {
            return RuntimePathKind::ToolkitBin;
        }
        return RuntimePathKind::ToolkitRoot;
    }
    return RuntimePathKind::Generic;
}

struct RuntimeCandidate {
    std::filesystem::path path;
    int version_token{0};
    int version_rank{0};
    RuntimeDirectorySource source{RuntimeDirectorySource::Path};
    RuntimePathKind path_kind{RuntimePathKind::Generic};
};

std::vector<RuntimeCandidate> sort_runtime_candidates(std::vector<RuntimeCandidate> candidates)
{
    std::sort(candidates.begin(), candidates.end(), [](const RuntimeCandidate& lhs, const RuntimeCandidate& rhs) {
        const int lhs_source_priority = runtime_directory_source_priority(lhs.source);
        const int rhs_source_priority = runtime_directory_source_priority(rhs.source);
        if (lhs_source_priority != rhs_source_priority) {
            return lhs_source_priority < rhs_source_priority;
        }

        const int lhs_kind_priority = runtime_path_kind_priority(lhs.path_kind);
        const int rhs_kind_priority = runtime_path_kind_priority(rhs.path_kind);
        if (lhs_kind_priority != rhs_kind_priority) {
            return lhs_kind_priority < rhs_kind_priority;
        }

        if (lhs.version_rank != rhs.version_rank) {
            return lhs.version_rank > rhs.version_rank;
        }

        if (lhs.version_token != rhs.version_token) {
            return lhs.version_token > rhs.version_token;
        }

        return normalize_key(lhs.path) < normalize_key(rhs.path);
    });

    return candidates;
}

void append_unique(std::vector<RuntimeDirectoryCandidate>& paths,
                   const std::filesystem::path& candidate,
                   RuntimeDirectorySource source,
                   bool require_exists = true)
{
    if (candidate.empty()) {
        return;
    }

    std::error_code ec;
    if (require_exists && !std::filesystem::exists(candidate, ec)) {
        return;
    }

    const std::filesystem::path normalized = candidate.lexically_normal();
    const std::wstring key = normalize_key(normalized);
    const auto duplicate = std::find_if(paths.begin(), paths.end(), [&](const RuntimeDirectoryCandidate& existing) {
        return normalize_key(existing.path) == key;
    });
    if (duplicate == paths.end()) {
        paths.push_back(RuntimeDirectoryCandidate{normalized, source});
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

void add_cuda_root(std::vector<RuntimeDirectoryCandidate>& directories,
                   const std::filesystem::path& root,
                   RuntimeDirectorySource source)
{
    if (root.empty()) {
        return;
    }

    const std::filesystem::path bin_x64_dir = root / L"bin" / L"x64";
    const std::filesystem::path bin_dir = root / L"bin";
    std::error_code ec;
    if (std::filesystem::exists(bin_x64_dir, ec)) {
        append_unique(directories, bin_x64_dir, source);
    }
    ec.clear();
    if (std::filesystem::exists(bin_dir, ec)) {
        append_unique(directories, bin_dir, source);
    }

    append_unique(directories, root, source);
}

std::vector<RuntimeDirectoryCandidate> path_entries()
{
    std::vector<RuntimeDirectoryCandidate> entries;
    const auto path_env = read_env_var(L"PATH");
    if (!path_env.has_value()) {
        return entries;
    }

    size_t start = 0;
    while (start <= path_env->size()) {
        const size_t end = path_env->find(L';', start);
        const std::wstring token = path_env->substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!token.empty()) {
            append_unique(entries, std::filesystem::path(token), RuntimeDirectorySource::Path);
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }

    return entries;
}

void add_cuda_env_roots(std::vector<RuntimeDirectoryCandidate>& directories)
{
    if (const auto cuda_path = read_env_var(L"CUDA_PATH")) {
        add_cuda_root(directories, std::filesystem::path(*cuda_path), RuntimeDirectorySource::ToolkitHint);
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
                add_cuda_root(directories,
                              std::filesystem::path(entry.substr(separator + 1)),
                              RuntimeDirectorySource::ToolkitHint);
            }
        }
        current += entry.size() + 1;
    }

    FreeEnvironmentStringsW(environment_block);
}

void add_default_cuda_roots(std::vector<RuntimeDirectoryCandidate>& directories)
{
    const std::filesystem::path toolkit_root = L"C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA";
    std::error_code ec;
    if (!std::filesystem::exists(toolkit_root, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(
             toolkit_root,
             kDirectoryIteratorOptions,
             ec)) {
        if (ec) {
            break;
        }
        std::error_code entry_ec;
        if (!entry.is_directory(entry_ec) || entry_ec) {
            continue;
        }
        add_cuda_root(directories, entry.path(), RuntimeDirectorySource::ToolkitHint);
    }
}

std::vector<RuntimeDirectoryCandidate> build_candidate_runtime_directories(const ProbeOptions& options)
{
    std::vector<RuntimeDirectoryCandidate> directories;
    if (options.ggml_directory.has_value()) {
        append_unique(directories,
                      *options.ggml_directory,
                      RuntimeDirectorySource::Packaged,
                      false);
    }
    for (const auto& directory : options.preferred_runtime_directories) {
        append_unique(directories,
                      directory,
                      RuntimeDirectorySource::Packaged,
                      false);
    }

    if (options.include_system_directories) {
        add_cuda_env_roots(directories);
        add_default_cuda_roots(directories);

        for (const auto& entry : path_entries()) {
            append_unique(directories, entry.path, entry.source);
        }
    }

    return directories;
}

std::vector<RuntimeCandidate> candidate_runtime_libraries(const ProbeOptions& options)
{
    std::vector<RuntimeCandidate> candidates;

    for (const auto& directory : build_candidate_runtime_directories(options)) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(
                 directory.path,
                 kDirectoryIteratorOptions,
                 ec)) {
            if (ec) {
                break;
            }
            std::error_code entry_ec;
            if (!entry.is_regular_file(entry_ec) || entry_ec) {
                continue;
            }

            const auto version_info = parse_runtime_version_token_info(entry.path());
            if (!version_info.has_value()) {
                continue;
            }

            const std::wstring key = normalize_key(entry.path());
            const auto duplicate = std::find_if(candidates.begin(), candidates.end(), [&](const RuntimeCandidate& candidate) {
                return normalize_key(candidate.path) == key;
            });
            if (duplicate == candidates.end()) {
                int version_rank = version_info->rank;
                if (const auto toolkit_rank = parse_toolkit_version_rank(entry.path())) {
                    version_rank = *toolkit_rank;
                }
                candidates.push_back(RuntimeCandidate{
                    entry.path().lexically_normal(),
                    version_info->token,
                    version_rank,
                    directory.source,
                    classify_runtime_path(entry.path()),
                });
            }
        }
    }

    return sort_runtime_candidates(std::move(candidates));
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
                           const std::vector<std::filesystem::path>& dll_directories)
{
    if (ggml_directory.empty()) {
        return false;
    }

    const std::filesystem::path backend_path = ggml_directory / L"ggml-cuda.dll";
    std::error_code ec;
    if (!std::filesystem::exists(backend_path, ec)) {
        return false;
    }

    std::vector<std::filesystem::path> directories;
    directories.reserve(dll_directories.size() + 1);
    directories.push_back(ggml_directory);
    for (const auto& directory : dll_directories) {
        if (!directory.empty()) {
            directories.push_back(directory);
        }
    }

    const auto cookies = add_user_directories(directories);
    (void) cookies;

    LibraryHandle backend(LoadLibraryExW(
        backend_path.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    return backend.valid();
}

RuntimeSource to_public_source(RuntimeDirectorySource source)
{
    switch (source) {
    case RuntimeDirectorySource::Packaged:
        return RuntimeSource::Packaged;
    case RuntimeDirectorySource::ToolkitHint:
        return RuntimeSource::ToolkitHint;
    case RuntimeDirectorySource::Path:
        return RuntimeSource::Path;
    }
    return RuntimeSource::None;
}

ProbeResult probe_impl(const ProbeOptions& options)
{
    ProbeResult result;

    LibraryHandle driver(LoadLibraryExW(
        L"nvcuda.dll",
        nullptr,
        LOAD_LIBRARY_SEARCH_SYSTEM32));
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

    const auto candidates = candidate_runtime_libraries(options);

    for (const auto& candidate : candidates) {
        result.runtime_present = true;
        result.runtime_source = to_public_source(candidate.source);

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

        if (!options.ggml_directory.has_value() || options.ggml_directory->empty()) {
            result.backend_loadable = true;
            return result;
        }

        std::vector<std::filesystem::path> backend_directories = options.preferred_runtime_directories;
        backend_directories.push_back(candidate.path.parent_path());
        if (can_load_cuda_backend(*options.ggml_directory, backend_directories)) {
            result.backend_loadable = true;
            return result;
        }
    }

    if (!result.driver_present) {
        result.failure_reason = "driver_missing";
    } else if (!result.driver_initialized || result.device_count <= 0) {
        result.failure_reason = "driver_no_device";
    } else if (!result.runtime_present) {
        result.failure_reason = "runtime_missing";
    } else if (!result.runtime_usable) {
        result.failure_reason = "runtime_unusable";
    } else if (options.ggml_directory.has_value() && !options.ggml_directory->empty()) {
        result.failure_reason = "backend_dependency_mismatch";
    }
    return result;
}

} // namespace
#endif

ProbeResult probe(const std::optional<std::filesystem::path>& ggml_directory)
{
#ifdef _WIN32
    ProbeOptions options;
    options.ggml_directory = ggml_directory;
    if (ggml_directory.has_value() && !ggml_directory->empty()) {
        options.preferred_runtime_directories.push_back(*ggml_directory);
    }
    return probe_impl(options);
#else
    (void) ggml_directory;
    return ProbeResult{};
#endif
}

ProbeResult probe(const ProbeOptions& options)
{
#ifdef _WIN32
    return probe_impl(options);
#else
    (void) options;
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

std::optional<std::filesystem::path> best_runtime_library_path(const ProbeOptions& options)
{
    const ProbeResult result = probe(options);
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

int installed_runtime_version_token(const ProbeOptions& options)
{
    const ProbeResult result = probe(options);
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

std::string best_runtime_library_name(const ProbeOptions& options)
{
    const auto path = best_runtime_library_path(options);
    if (!path.has_value()) {
        return std::string();
    }
    return path->filename().string();
}

std::string runtime_source_name(RuntimeSource source)
{
    switch (source) {
    case RuntimeSource::None:
        return "none";
    case RuntimeSource::Packaged:
        return "packaged";
    case RuntimeSource::ToolkitHint:
        return "toolkit";
    case RuntimeSource::Path:
        return "path";
    }
    return "unknown";
}

#ifdef AI_FILE_SORTER_TEST_BUILD
namespace TestAccess {

int runtime_version_rank(std::string_view file_name)
{
#ifdef _WIN32
    const auto parsed = parse_runtime_version_token_info(std::filesystem::path(std::string(file_name)));
    return parsed.has_value() ? parsed->rank : 0;
#else
    (void) file_name;
    return 0;
#endif
}

std::vector<std::filesystem::path> rank_runtime_candidates(
    const std::vector<std::filesystem::path>& runtime_paths)
{
    std::vector<std::filesystem::path> ranked_paths;
#ifdef _WIN32
    std::vector<RuntimeCandidate> candidates;
    candidates.reserve(runtime_paths.size());

    for (const auto& path : runtime_paths) {
        const auto version_info = parse_runtime_version_token_info(path);
        if (!version_info.has_value()) {
            continue;
        }

        int version_rank = version_info->rank;
        if (const auto toolkit_rank = parse_toolkit_version_rank(path)) {
            version_rank = *toolkit_rank;
        }

        candidates.push_back(RuntimeCandidate{
            path.lexically_normal(),
            version_info->token,
            version_rank,
            RuntimeDirectorySource::Path,
            classify_runtime_path(path),
        });
    }

    candidates = sort_runtime_candidates(std::move(candidates));
    ranked_paths.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        ranked_paths.push_back(candidate.path);
    }
#else
    ranked_paths = runtime_paths;
#endif
    return ranked_paths;
}

std::vector<std::filesystem::path> rank_runtime_candidates_with_sources(
    const std::vector<std::filesystem::path>& packaged_runtime_paths,
    const std::vector<std::filesystem::path>& toolkit_hint_runtime_paths,
    const std::vector<std::filesystem::path>& path_runtime_paths)
{
    std::vector<std::filesystem::path> ranked_paths;
#ifdef _WIN32
    std::vector<RuntimeCandidate> candidates;
    candidates.reserve(packaged_runtime_paths.size() +
                       toolkit_hint_runtime_paths.size() +
                       path_runtime_paths.size());

    const auto add_candidate = [&](const std::filesystem::path& path,
                                   RuntimeDirectorySource source) {
        const auto version_info = parse_runtime_version_token_info(path);
        if (!version_info.has_value()) {
            return;
        }

        int version_rank = version_info->rank;
        if (const auto toolkit_rank = parse_toolkit_version_rank(path)) {
            version_rank = *toolkit_rank;
        }

        candidates.push_back(RuntimeCandidate{
            path.lexically_normal(),
            version_info->token,
            version_rank,
            source,
            classify_runtime_path(path),
        });
    };

    for (const auto& path : packaged_runtime_paths) {
        add_candidate(path, RuntimeDirectorySource::Packaged);
    }
    for (const auto& path : toolkit_hint_runtime_paths) {
        add_candidate(path, RuntimeDirectorySource::ToolkitHint);
    }
    for (const auto& path : path_runtime_paths) {
        add_candidate(path, RuntimeDirectorySource::Path);
    }

    candidates = sort_runtime_candidates(std::move(candidates));
    ranked_paths.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        ranked_paths.push_back(candidate.path);
    }
#else
    ranked_paths.reserve(packaged_runtime_paths.size() +
                         toolkit_hint_runtime_paths.size() +
                         path_runtime_paths.size());
    ranked_paths.insert(ranked_paths.end(), packaged_runtime_paths.begin(), packaged_runtime_paths.end());
    ranked_paths.insert(ranked_paths.end(), toolkit_hint_runtime_paths.begin(), toolkit_hint_runtime_paths.end());
    ranked_paths.insert(ranked_paths.end(), path_runtime_paths.begin(), path_runtime_paths.end());
#endif
    return ranked_paths;
}

std::vector<std::filesystem::path> candidate_runtime_directories(const ProbeOptions& options)
{
    std::vector<std::filesystem::path> paths;
#ifdef _WIN32
    const auto candidates = build_candidate_runtime_directories(options);
    paths.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        paths.push_back(candidate.path);
    }
#else
    for (const auto& directory : options.preferred_runtime_directories) {
        paths.push_back(directory);
    }
#endif
    return paths;
}

} // namespace TestAccess
#endif

} // namespace WindowsCudaProbe
