#include "Utils.hpp"
#include "Logger.hpp"
#include "TestHooks.hpp"
#include "WindowsCudaProbe.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>  // for memset
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <vector>
#include <optional>
#include <mutex>
#include <functional>
#include <QCoreApplication>
#include <QMetaObject>
#include <QFile>
#include <QString>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace {
constexpr std::size_t kFormatSizeBufferBytes = 64;
constexpr double kBytesPerKiB = 1024.0;
constexpr double kBytesPerMiB = 1024.0 * kBytesPerKiB;
constexpr double kBytesPerGiB = 1024.0 * kBytesPerMiB;
constexpr std::size_t kBytesPerMiBInt = 1024U * 1024U;
constexpr int kMinimumCudaVramForGpuLayersMb = 2048;
constexpr int kCudaVramStepMb = 512;
constexpr int kCudaBaseGpuLayers = 14;
constexpr int kCudaLayersPerStep = 2;
constexpr int kCudaMaxGpuLayers = 32;

std::mutex& llm_storage_override_mutex()
{
    static std::mutex mutex;
    return mutex;
}

std::string& llm_storage_directory_override()
{
    static std::string path;
    return path;
}

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

template <typename... Args>
void log_core(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}

std::string to_forward_slashes(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

std::string trim_leading_separators(std::string value) {
    auto is_separator = [](char ch) {
        return ch == '/' || ch == '\\';
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](char ch) { return !is_separator(ch); }));
    return value;
}

std::optional<std::filesystem::path> try_utf8_to_path(const std::string& value) {
    try {
        return Utils::utf8_to_path(value);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<std::string> collect_user_prefixes() {
    std::vector<std::string> prefixes;

    auto append = [&](const char* candidate) {
        if (!candidate || *candidate == '\0') {
            return;
        }
        const std::string raw(candidate);
        if (auto converted = try_utf8_to_path(raw)) {
            prefixes.push_back(to_forward_slashes(Utils::path_to_utf8(*converted)));
        } else {
            prefixes.push_back(to_forward_slashes(raw));
        }
    };

    append(std::getenv("HOME"));
    append(std::getenv("USERPROFILE"));

    if (prefixes.empty() && Utils::is_os_windows()) {
        if (const char* username = std::getenv("USERNAME")) {
            prefixes.emplace_back(std::string("C:/Users/") + username);
        }
    }

    return prefixes;
}

std::function<bool()>& cuda_availability_probe() {
    static std::function<bool()> probe;
    return probe;
}

std::function<std::optional<Utils::CudaMemoryInfo>()>& cuda_memory_probe() {
    static std::function<std::optional<Utils::CudaMemoryInfo>()> probe;
    return probe;
}

std::optional<std::string> strip_prefix(const std::string& path,
                                        const std::vector<std::string>& prefixes) {
    for (const auto& original_prefix : prefixes) {
        if (original_prefix.empty()) {
            continue;
        }
        std::string prefix = original_prefix;
        if (prefix.back() != '/') {
            prefix.push_back('/');
        }
        if (path.size() < prefix.size()) {
            continue;
        }
        if (!std::equal(prefix.begin(), prefix.end(), path.begin())) {
            continue;
        }

        std::string trimmed = trim_leading_separators(path.substr(prefix.size()));
        if (!trimmed.empty()) {
            return trimmed;
        }
    }

    return std::nullopt;
}
}
#ifdef _WIN32
    #include <windows.h>
    #include <wininet.h>
#elif __linux__
    #include <dlfcn.h>
    #include <limits.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/socket.h>
#elif __APPLE__
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
    #include <limits.h>
    #include <netdb.h>
    #include <sys/socket.h>
#endif
#include <iostream>
#include <Types.hpp>
#include <cstddef>
#include <stdexcept>
#ifdef _WIN32
    #include <appmodel.h>
    #include <cwchar>
#endif

namespace {

bool is_nonempty_file(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return !ec && size > 0;
}

QString path_to_qstring(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

#ifdef _WIN32
std::optional<std::wstring> current_package_family_name()
{
    UINT32 length = 0;
    LONG rc = GetCurrentPackageFamilyName(&length, nullptr);
    if (rc == APPMODEL_ERROR_NO_PACKAGE) {
        return std::nullopt;
    }
    if (rc != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }

    std::wstring family;
    family.resize(length);
    rc = GetCurrentPackageFamilyName(&length, family.data());
    if (rc != ERROR_SUCCESS) {
        return std::nullopt;
    }
    if (length > 0 && family[length - 1] == L'\0') {
        family.resize(length - 1);
    }

    return family;
}

std::optional<std::filesystem::path> packaged_local_cache_root()
{
    const auto family = current_package_family_name();
    if (!family) {
        return std::nullopt;
    }

    const wchar_t* local_app_data = _wgetenv(L"LOCALAPPDATA");
    if (!local_app_data || *local_app_data == L'\0') {
        return std::nullopt;
    }

    return std::filesystem::path(local_app_data) / L"Packages" / std::filesystem::path(*family) /
           L"LocalCache" / L"aifilesorter";
}
#endif

std::filesystem::path user_writable_app_data_dir()
{
    constexpr const char* kAppName = "AIFileSorter";
    if (const char* override_root = std::getenv("AI_FILE_SORTER_CONFIG_DIR");
        override_root && *override_root) {
        return std::filesystem::path(override_root) / kAppName;
    }

#ifdef _WIN32
    if (auto packaged_root = packaged_local_cache_root()) {
        return *packaged_root;
    }

    if (const char* app_data = std::getenv("APPDATA"); app_data && *app_data) {
        return std::filesystem::path(app_data) / kAppName;
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / kAppName;
    }
#else
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".config" / kAppName;
    }
#endif

    throw std::runtime_error("Unable to determine writable app data directory for CA bundle");
}

std::filesystem::path bundled_ca_bundle_path()
{
    const std::filesystem::path exe_path = std::filesystem::path(Utils::get_executable_path());
    return exe_path.parent_path() / "certs" / "cacert.pem";
}

std::filesystem::path staged_ca_bundle_path()
{
    return user_writable_app_data_dir() / "certs" / "cacert.pem";
}

bool ca_bundle_staging_forced()
{
    const char* override_root = std::getenv("AI_FILE_SORTER_CONFIG_DIR");
    return override_root && *override_root;
}

QByteArray read_embedded_ca_bundle()
{
    QFile resource(QStringLiteral(":/dev/hfstudio/AIFileSorter/certs/cacert.pem"));
    if (!resource.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open embedded CA bundle resource");
    }

    QByteArray data = resource.readAll();
    resource.close();
    if (data.isEmpty()) {
        throw std::runtime_error("Embedded CA bundle resource is empty");
    }

    return data;
}

void write_ca_bundle(const std::filesystem::path& cert_file, const QByteArray& data)
{
    std::error_code ec;
    std::filesystem::create_directories(cert_file.parent_path(), ec);
    if (ec) {
        throw std::system_error(ec, "Failed to create CA bundle directory");
    }

    QFile output(path_to_qstring(cert_file));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error("Failed to create CA bundle file at " +
                                 Utils::path_to_utf8(cert_file));
    }
    if (output.write(data) != data.size()) {
        output.close();
        throw std::runtime_error("Failed to write CA bundle file at " +
                                 Utils::path_to_utf8(cert_file));
    }
    output.close();
}

std::filesystem::path resolve_ca_bundle()
{
    if (!ca_bundle_staging_forced()) {
        const auto bundled = bundled_ca_bundle_path();
        if (is_nonempty_file(bundled)) {
            return bundled;
        }
    }

    const auto staged = staged_ca_bundle_path();
    const auto data = read_embedded_ca_bundle();
    std::error_code ec;
    const auto current_size = std::filesystem::file_size(staged, ec);
    if (ec || current_size != static_cast<decltype(current_size)>(data.size())) {
        write_ca_bundle(staged, data);
    }

    return staged;
}

struct CaBundleCache {
    std::mutex mutex;
    bool initialized{false};
    std::filesystem::path cached_path;
    std::exception_ptr init_error;
};

CaBundleCache& ca_bundle_cache()
{
    static CaBundleCache cache;
    return cache;
}

} // namespace

// Shortcuts for loading libraries on different OSes
#ifdef _WIN32
    using LibraryHandle = HMODULE;

    LibraryHandle loadLibrary(const char* name) {
        return LoadLibraryA(name);
    }

    LibraryHandle loadLibrary(const std::filesystem::path& path) {
        return LoadLibraryExW(
            path.c_str(),
            nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    void* getSymbol(LibraryHandle lib, const char* symbol) {
        return reinterpret_cast<void*>(GetProcAddress(lib, symbol));
    }

    void closeLibrary(LibraryHandle lib) {
        FreeLibrary(lib);
    }
#else
    using LibraryHandle = void*;

    LibraryHandle loadLibrary(const char* name) {
        return dlopen(name, RTLD_LAZY);
    }

    void* getSymbol(LibraryHandle lib, const char* symbol) {
        return dlsym(lib, symbol);
    }

    void closeLibrary(LibraryHandle lib) {
        dlclose(lib);
    }
#endif


bool Utils::is_network_available()
{
#ifdef _WIN32
    DWORD flags;
    return InternetGetConnectedState(&flags, 0);
#else
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const int rc = getaddrinfo("www.google.com", "80", &hints, &result);
    if (result) {
        freeaddrinfo(result);
    }
    return rc == 0;
#endif
}


std::string Utils::get_executable_path()
{
#ifdef _WIN32
    char result[MAX_PATH];
    GetModuleFileNameA(NULL, result, MAX_PATH);
    return std::string(result);
#elif __linux__
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return (count != -1) ? std::string(result, count) : "";
#elif __APPLE__
    char result[PATH_MAX];
    uint32_t size = sizeof(result);
    if (_NSGetExecutablePath(result, &size) == 0) {
        return std::string(result);
    } else {
        throw std::runtime_error("Path buffer too small");
    }
#else
    throw std::runtime_error("Unsupported platform");
#endif
}


std::filesystem::path Utils::ensure_ca_bundle() {
    auto& cache = ca_bundle_cache();
    std::lock_guard<std::mutex> lock(cache.mutex);

    if (!cache.initialized) {
        cache.initialized = true;
        try {
            cache.cached_path = resolve_ca_bundle();
            cache.init_error = nullptr;
        } catch (...) {
            cache.init_error = std::current_exception();
        }
    }

    if (cache.init_error) {
        std::rethrow_exception(cache.init_error);
    }

    return cache.cached_path;
}


std::string Utils::path_to_utf8(const std::filesystem::path& path) {
#ifdef _WIN32
    if (path.empty()) {
        return {};
    }

    const std::wstring native = path.native();
    if (native.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8, 0, native.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        throw std::system_error(
            std::error_code(GetLastError(), std::system_category()),
            "WideCharToMultiByte failed while converting path to UTF-8");
    }

    std::string buffer(static_cast<std::size_t>(required - 1), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8, 0, native.c_str(), -1, buffer.data(), required, nullptr, nullptr);
    if (written <= 0) {
        throw std::system_error(
            std::error_code(GetLastError(), std::system_category()),
            "WideCharToMultiByte failed while converting path to UTF-8");
    }

    buffer.resize(static_cast<std::size_t>(written - 1));
    return buffer;
#else
    return path.generic_string();
#endif
}


std::filesystem::path Utils::utf8_to_path(const std::string& utf8_path) {
#ifdef _WIN32
    if (utf8_path.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        throw std::system_error(
            std::error_code(GetLastError(), std::system_category()),
            "MultiByteToWideChar failed while converting UTF-8 to path");
    }

    std::wstring buffer(static_cast<std::size_t>(required - 1), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path.c_str(), -1, buffer.data(), required);
    if (written <= 0) {
        throw std::system_error(
            std::error_code(GetLastError(), std::system_category()),
            "MultiByteToWideChar failed while converting UTF-8 to path");
    }

    buffer.resize(static_cast<std::size_t>(written - 1));
    return std::filesystem::path(buffer);
#else
    return std::filesystem::path(utf8_path);
#endif
}


bool Utils::is_valid_directory(const char *path)
{
    if (!path || *path == '\0') {
        return false;
    }
#ifdef _WIN32
    std::filesystem::path fs_path;
    try {
        fs_path = utf8_to_path(path);
    } catch (const std::exception&) {
        return false;
    }
#else
    std::filesystem::path fs_path(path);
#endif

    std::error_code ec;
    return std::filesystem::is_directory(fs_path, ec);
}

namespace {
int hex_char_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

unsigned char combine_hex_pair(char high, char low)
{
    const int hi = hex_char_value(high);
    const int lo = hex_char_value(low);
    if (hi < 0 || lo < 0) {
        throw std::invalid_argument("Hex string contains invalid characters");
    }
    return static_cast<unsigned char>((hi << 4) | lo);
}
}


std::vector<unsigned char> Utils::hex_to_vector(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }

    std::vector<unsigned char> data;
    data.reserve(hex.size() / 2);

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        data.push_back(combine_hex_pair(hex[i], hex[i + 1]));
    }

    return data;
}


const char* Utils::to_cstr(const std::u8string& u8str) {
    return reinterpret_cast<const char*>(u8str.c_str());
}


void Utils::ensure_directory_exists(const std::string &dir)
{
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    } catch (const std::exception &e) {
        log_core(spdlog::level::err, "Error creating log directory: {}", e.what());
        throw;
    }
}


bool Utils::is_os_windows() {
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

    
bool Utils::is_os_macos() {
#if defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

    
bool Utils::is_os_linux() {
#if defined(__linux__)
    return true;
#else
    return false;
#endif
}


std::string Utils::format_size(curl_off_t bytes)
{
    char buffer[kFormatSizeBufferBytes];
    if (bytes >= static_cast<curl_off_t>(kBytesPerGiB))
        snprintf(buffer, sizeof(buffer), "%.2f GB",
                 bytes / kBytesPerGiB);
    else if (bytes >= static_cast<curl_off_t>(kBytesPerMiB))
        snprintf(buffer, sizeof(buffer), "%.2f MB", bytes / kBytesPerMiB);
    else if (bytes >= static_cast<curl_off_t>(kBytesPerKiB))
        snprintf(buffer, sizeof(buffer), "%.2f KB", bytes / kBytesPerKiB);
    else
        snprintf(buffer, sizeof(buffer), "%.2f B", static_cast<double>(bytes));
    return buffer;
}


int Utils::get_ngl(int vram_mb) {
    if (vram_mb < kMinimumCudaVramForGpuLayersMb) return 0;

    const int step = (vram_mb - kMinimumCudaVramForGpuLayersMb) / kCudaVramStepMb;
    return std::min(kCudaBaseGpuLayers + step * kCudaLayersPerStep, kCudaMaxGpuLayers);
}


std::optional<Utils::CudaMemoryInfo> Utils::query_cuda_memory() {
    if (auto& probe = cuda_memory_probe()) {
        return probe();
    }
#ifdef _WIN32
    const auto runtime_path = WindowsCudaProbe::best_runtime_library_path();
    if (!runtime_path.has_value()) {
        log_core(spdlog::level::err, "Failed to locate a usable CUDA runtime library.");
        return std::nullopt;
    }
    LibraryHandle lib = loadLibrary(*runtime_path);
#else
    LibraryHandle lib = loadLibrary("libcudart.so");
    if (!lib) {
        lib = loadLibrary("libcudart.so.12");
    }
#endif

    if (!lib) {
        log_core(spdlog::level::err, "Failed to load CUDA runtime library.");
        return std::nullopt;
    }

    using cudaMemGetInfo_t = int (*)(size_t*, size_t*);
    using cudaGetDeviceProperties_t = int (*)(void*, int);

    auto cudaMemGetInfo = reinterpret_cast<cudaMemGetInfo_t>(getSymbol(lib, "cudaMemGetInfo"));
    auto cudaGetDeviceProperties = reinterpret_cast<cudaGetDeviceProperties_t>(getSymbol(lib, "cudaGetDeviceProperties"));

    if (!cudaMemGetInfo) {
        log_core(spdlog::level::err, "Failed to resolve required CUDA runtime symbols.");
        closeLibrary(lib);
        return std::nullopt;
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    size_t device_total_bytes = 0;

    if (cudaMemGetInfo(&free_bytes, &total_bytes) != 0) {
        log_core(spdlog::level::warn, "Warning: cudaMemGetInfo failed");
        free_bytes = 0;
        total_bytes = 0;
    }

    if (cudaGetDeviceProperties) {
        // Query total device memory from cudaGetDeviceProperties for reference.
        constexpr size_t cudaDevicePropSize = 2560;
        alignas(std::max_align_t) uint8_t prop_buffer[cudaDevicePropSize];
        std::memset(prop_buffer, 0, sizeof(prop_buffer));
        if (cudaGetDeviceProperties(prop_buffer, 0) == 0) {
            struct DevicePropShim {
                char name[256];
                size_t totalGlobalMem;
            };
            auto *prop = reinterpret_cast<DevicePropShim*>(prop_buffer);
            device_total_bytes = prop->totalGlobalMem;
            constexpr size_t kMinReasonableBytes = 64ULL * 1024ULL * 1024ULL;
            constexpr size_t kMaxReasonableBytes = 1ULL << 50; // 1 PiB
            if (device_total_bytes < kMinReasonableBytes ||
                device_total_bytes > kMaxReasonableBytes) {
                device_total_bytes = 0;
            }
        }
    }

    if (total_bytes == 0 && device_total_bytes != 0) {
        total_bytes = device_total_bytes;
    }

    closeLibrary(lib);

    if (free_bytes == 0 && total_bytes == 0) {
        log_core(spdlog::level::warn, "CUDA memory metrics unavailable (both free and total bytes are zero).");
        return std::nullopt;
    }

    CudaMemoryInfo info;
    info.free_bytes = free_bytes;
    info.total_bytes = total_bytes;
    info.device_total_bytes = device_total_bytes;
    return info;
}


int Utils::compute_ngl_from_cuda_memory(const CudaMemoryInfo& info) {
    size_t usable_bytes = (info.free_bytes > 0) ? info.free_bytes : info.total_bytes;
    if (usable_bytes == 0) {
        return 0;
    }
    const int vram_mb = static_cast<int>(usable_bytes / kBytesPerMiBInt);
    return get_ngl(vram_mb);
}


int Utils::determine_ngl_cuda() {
    auto info = query_cuda_memory();
    if (!info.has_value()) {
        return 0;
    }

    return compute_ngl_from_cuda_memory(*info);
}


#ifdef _WIN32
std::optional<std::filesystem::path> packaged_llm_path()
{
    if (auto root = packaged_local_cache_root()) {
        return *root / L"llms";
    }
    return std::nullopt;
}
#endif

template <typename Func>
void Utils::run_on_main_thread(Func&& func)
{
    auto task = std::make_shared<std::function<void()>>(std::forward<Func>(func));
    if (auto* app = QCoreApplication::instance()) {
        QMetaObject::invokeMethod(app, [task]() { (*task)(); }, Qt::QueuedConnection);
    } else {
        (*task)();
    }
}

void Utils::set_llm_storage_directory_override(const std::string& path)
{
    std::lock_guard<std::mutex> lock(llm_storage_override_mutex());
    llm_storage_directory_override() = trim_copy(path);
}

std::string Utils::get_llm_storage_directory_override()
{
    std::lock_guard<std::mutex> lock(llm_storage_override_mutex());
    return llm_storage_directory_override();
}

std::string Utils::get_default_llm_destination()
{
    const std::string configured = get_llm_storage_directory_override();
    if (!configured.empty()) {
        return configured;
    }

    if (const char* env_override = std::getenv("AI_FILE_SORTER_LLM_STORAGE_DIR");
        env_override && *env_override) {
        return trim_copy(env_override);
    }
    if (const char* env_override = std::getenv("AI_FILE_SORTER_LLM_DIR");
        env_override && *env_override) {
        return trim_copy(env_override);
    }

    const char* home = std::getenv("HOME");

    if (Utils::is_os_windows()) {
        const char* appdata = std::getenv("APPDATA");
        if (!appdata) throw std::runtime_error("APPDATA not set");
        std::filesystem::path legacy = std::filesystem::path(appdata) / "aifilesorter" / "llms";

#ifdef _WIN32
        if (auto packaged = packaged_llm_path()) {
            // Prefer the packaged LocalCache path; fall back to legacy if it already exists (backward compatibility).
            if (std::filesystem::exists(legacy)) {
                return legacy.string();
            }
            return packaged->string();
        }
#endif
        return legacy.string();
    }

    if (!home) throw std::runtime_error("HOME not set");

    if (Utils::is_os_macos()) {
        return (std::filesystem::path(home) / "Library" / "Application Support" / "aifilesorter" / "llms").string();
    }

    return (std::filesystem::path(home) / ".local" / "share" / "aifilesorter" / "llms").string();
}


std::string Utils::get_file_name_from_url(std::string url)
{
    // Strip fragment (#...) and query (?...) before extracting filename
    auto fragment_pos = url.find('#');
    if (fragment_pos != std::string::npos) {
        url = url.substr(0, fragment_pos);
    }
    auto query_pos = url.find('?');
    if (query_pos != std::string::npos) {
        url = url.substr(0, query_pos);
    }

    auto last_slash = url.find_last_of('/');
    if (last_slash == std::string::npos || last_slash == url.length() - 1) {
        throw std::runtime_error("Invalid download URL: can't extract filename");
    }
    std::string filename = url.substr(last_slash + 1);

    if (filename.empty()) {
        throw std::runtime_error("Invalid download URL: empty filename");
    }

    return filename;
}


std::string Utils::make_default_path_to_file_from_download_url(std::string url)
{
    std::string filename = get_file_name_from_url(url);
    std::string path_to_file = (std::filesystem::path(get_default_llm_destination()) / filename).string();
    return path_to_file;
}


namespace {
using cudaGetDeviceCount_t = int (*)(int*);
using cudaSetDevice_t = int (*)(int);
using cudaMemGetInfo_t = int (*)(size_t*, size_t*);

LibraryHandle open_cuda_runtime() {
#ifdef _WIN32
    std::string dllName = Utils::get_cudart_dll_name();
    if (dllName.empty()) {
        log_core(spdlog::level::warn, "[CUDA] DLL name is empty — likely failed to get CUDA version.");
        return nullptr;
    }
    LibraryHandle handle = loadLibrary(dllName.c_str());
    log_core(spdlog::level::info, "[CUDA] Trying to load: {} => {}", dllName, handle ? "Success" : "Failure");
    return handle;
#else
    return loadLibrary("libcudart.so");
#endif
}

bool resolve_cuda_symbols(LibraryHandle handle,
                          cudaGetDeviceCount_t& get_device_count,
                          cudaSetDevice_t& set_device,
                          cudaMemGetInfo_t& mem_get_info) {
    get_device_count = reinterpret_cast<cudaGetDeviceCount_t>(getSymbol(handle, "cudaGetDeviceCount"));
    set_device = reinterpret_cast<cudaSetDevice_t>(getSymbol(handle, "cudaSetDevice"));
    mem_get_info = reinterpret_cast<cudaMemGetInfo_t>(getSymbol(handle, "cudaMemGetInfo"));

    log_core(spdlog::level::info, "[CUDA] Lookup cudaGetDeviceCount symbol: {}",
             get_device_count ? "Found" : "Not Found");

    return get_device_count && set_device && mem_get_info;
}
} // namespace

bool Utils::is_cuda_available() {
    log_core(spdlog::level::info, "[CUDA] Checking CUDA availability...");

    if (auto& probe = cuda_availability_probe()) {
        return probe();
    }

#ifdef _WIN32
    std::optional<std::filesystem::path> ggml_directory;
    if (const char* ggml_dir = std::getenv("AI_FILE_SORTER_GGML_DIR");
        ggml_dir && ggml_dir[0] != '\0') {
        ggml_directory = std::filesystem::path(std::string(ggml_dir));
    }

    const auto probe_result = WindowsCudaProbe::probe(ggml_directory);
    if (!probe_result.driver_present) {
        log_core(spdlog::level::warn, "[CUDA] Failed to load the NVIDIA CUDA driver (nvcuda.dll).");
        return false;
    }
    if (!probe_result.driver_initialized || probe_result.device_count <= 0) {
        log_core(spdlog::level::warn,
                 "[CUDA] NVIDIA driver is present but no usable CUDA device was initialized.");
        return false;
    }
    if (!probe_result.runtime_present) {
        log_core(spdlog::level::warn, "[CUDA] No CUDA runtime library (cudart64*.dll) was found.");
        return false;
    }
    if (!probe_result.runtime_usable) {
        log_core(spdlog::level::warn,
                 "[CUDA] CUDA runtime libraries were found, but none could be initialized successfully.");
        return false;
    }
    if (ggml_directory.has_value() && !probe_result.backend_loadable) {
        log_core(spdlog::level::warn,
                 "[CUDA] ggml-cuda.dll could not be loaded with the discovered CUDA runtime.");
        return false;
    }

    log_core(spdlog::level::info,
             "[CUDA] CUDA is available and {} device(s) found using runtime '{}'.",
             probe_result.device_count,
             probe_result.runtime_library_path.filename().string());
    return true;
#else

    LibraryHandle handle = open_cuda_runtime();
    if (!handle) {
        log_core(spdlog::level::warn, "[CUDA] Failed to load CUDA runtime library.");
        return false;
    }

    cudaGetDeviceCount_t cudaGetDeviceCount = nullptr;
    cudaSetDevice_t cudaSetDevice = nullptr;
    cudaMemGetInfo_t cudaMemGetInfo = nullptr;
    if (!resolve_cuda_symbols(handle, cudaGetDeviceCount, cudaSetDevice, cudaMemGetInfo)) {
        closeLibrary(handle);
        return false;
    }

    int count = 0;
    int status = cudaGetDeviceCount(&count);
    log_core(spdlog::level::info, "[CUDA] cudaGetDeviceCount returned status: {}, device count: {}", status, count);

    if (status != 0 || count == 0) {
        log_core(spdlog::level::warn,
                 status != 0 ? "[CUDA] CUDA error: {} from cudaGetDeviceCount" : "[CUDA] No CUDA devices found",
                 status);
        closeLibrary(handle);
        return false;
    }

    if (int set_status = cudaSetDevice(0); set_status != 0) {
        log_core(spdlog::level::warn, "[CUDA] Failed to set CUDA device 0 (error {})", set_status);
        closeLibrary(handle);
        return false;
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    if (int mem_status = cudaMemGetInfo(&free_bytes, &total_bytes); mem_status != 0) {
        log_core(spdlog::level::warn, "[CUDA] cudaMemGetInfo failed (error {})", mem_status);
        closeLibrary(handle);
        return false;
    }

    log_core(spdlog::level::info, "[CUDA] CUDA is available and {} device(s) found.", count);
    closeLibrary(handle);
    return true;
#endif
}

namespace TestHooks {

void set_cuda_availability_probe(CudaAvailabilityProbe probe) {
    cuda_availability_probe() = std::move(probe);
}

void reset_cuda_availability_probe() {
    cuda_availability_probe() = CudaAvailabilityProbe{};
}

void set_cuda_memory_probe(CudaMemoryProbe probe) {
    cuda_memory_probe() = std::move(probe);
}

void reset_cuda_memory_probe() {
    cuda_memory_probe() = CudaMemoryProbe{};
}

#ifdef AI_FILE_SORTER_TEST_BUILD
void reset_ca_bundle_cache()
{
    auto& cache = ca_bundle_cache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.initialized = false;
    cache.cached_path.clear();
    cache.init_error = nullptr;
}
#endif

} // namespace TestHooks

#ifdef _WIN32
int Utils::get_installed_cuda_runtime_version()
{
    const int version = WindowsCudaProbe::installed_runtime_version_token();
    if (version > 0) {
        log_core(spdlog::level::info, "[CUDA] Detected CUDA runtime token: {}", version);
    } else {
        log_core(spdlog::level::warn, "[CUDA] Unable to locate an installed CUDA runtime token.");
    }
    return version;
}
#endif


#ifdef _WIN32
std::string Utils::get_cudart_dll_name() {
    const std::string name = WindowsCudaProbe::best_runtime_library_name();
    if (!name.empty()) {
        log_core(spdlog::level::info, "[CUDA] Selected runtime DLL: {}", name);
    } else {
        log_core(spdlog::level::warn, "[CUDA] Unable to locate a compatible cudart64*.dll");
    }
    return name;
}
#endif


std::string Utils::abbreviate_user_path(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    const auto fs_path = try_utf8_to_path(path);
    if (!fs_path) {
        return trim_leading_separators(to_forward_slashes(path));
    }

    const std::filesystem::path normalized = fs_path->lexically_normal();
    const std::string generic_path = to_forward_slashes(Utils::path_to_utf8(normalized));

    const std::vector<std::string> prefixes = collect_user_prefixes();
    if (auto trimmed = strip_prefix(generic_path, prefixes)) {
        return *trimmed;
    }

    const std::string sanitized = trim_leading_separators(generic_path);
    if (!sanitized.empty()) {
        return sanitized;
    }

    return to_forward_slashes(Utils::path_to_utf8(normalized.filename()));
}

namespace {
std::string trim_ws(const std::string& value) {
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    const auto end = value.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) {
        return std::string();
    }
    return value.substr(start, end - start + 1);
}
}

std::string Utils::sanitize_path_label(const std::string& value) {
    const std::string invalid = R"(<>:"/\|?*)";
    QString normalized = QString::fromUtf8(value.c_str());
    normalized.remove(QChar::ReplacementCharacter);
    const QByteArray normalized_utf8 = normalized.normalized(QString::NormalizationForm_C).toUtf8();
    const std::string utf8_value(normalized_utf8.constData(),
                                 static_cast<std::size_t>(normalized_utf8.size()));
    std::string cleaned;
    cleaned.reserve(utf8_value.size());

    // Replace invalid path characters and control chars with spaces.
    for (unsigned char ch : utf8_value) {
        if (std::iscntrl(ch)) {
            continue;
        }
        if (invalid.find(static_cast<char>(ch)) != std::string::npos) {
            cleaned.push_back(' ');
        } else {
            cleaned.push_back(static_cast<char>(ch));
        }
    }

    // Collapse multiple spaces.
    std::string collapsed;
    collapsed.reserve(cleaned.size());
    bool prev_space = false;
    for (char ch : cleaned) {
        const bool is_space = std::isspace(static_cast<unsigned char>(ch));
        if (is_space) {
            if (!prev_space) {
                collapsed.push_back(' ');
            }
        } else {
            collapsed.push_back(ch);
        }
        prev_space = is_space;
    }

    // Trim and drop trailing dots/spaces (Windows safety).
    std::string trimmed = trim_ws(collapsed);
    while (!trimmed.empty() && (trimmed.back() == '.' || std::isspace(static_cast<unsigned char>(trimmed.back())))) {
        trimmed.pop_back();
    }

    return trim_ws(trimmed);
}
