#include "Utils.hpp"
#include "Logger.hpp"
#include "TestHooks.hpp"
#include <algorithm>
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

// Shortcuts for loading libraries on different OSes
#ifdef _WIN32
    using LibraryHandle = HMODULE;

    LibraryHandle loadLibrary(const char* name) {
        return LoadLibraryA(name);
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
    static std::once_flag init_flag;
    static std::filesystem::path cached_path;
    static std::exception_ptr init_error;

    std::call_once(init_flag, []() {
        try {
            std::filesystem::path exe_path = std::filesystem::path(get_executable_path());
            std::filesystem::path cert_dir = exe_path.parent_path() / "certs";
            std::filesystem::path cert_file = cert_dir / "cacert.pem";

            bool needs_write = true;
            if (std::filesystem::exists(cert_file)) {
                std::error_code ec;
                auto size = std::filesystem::file_size(cert_file, ec);
                needs_write = ec ? true : (size == 0);
            }

            if (needs_write) {
                ensure_directory_exists(cert_dir.string());

                QFile resource(QStringLiteral(":/net/quicknode/AIFileSorter/certs/cacert.pem"));
                if (!resource.open(QIODevice::ReadOnly)) {
                    throw std::runtime_error("Failed to open embedded CA bundle resource");
                }
                const QByteArray data = resource.readAll();
                resource.close();

                QFile output(QString::fromStdString(cert_file.string()));
                if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    throw std::runtime_error("Failed to create CA bundle file at " + cert_file.string());
                }
                if (output.write(data) != data.size()) {
                    output.close();
                    throw std::runtime_error("Failed to write CA bundle file at " + cert_file.string());
                }
                output.close();
            }

            cached_path = cert_file;
        } catch (...) {
            init_error = std::current_exception();
        }
    });

    if (init_error) {
        std::rethrow_exception(init_error);
    }

    return cached_path;
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
    char buffer[64];
    if (bytes >= (1LL << 30))
        snprintf(buffer, sizeof(buffer), "%.2f GB",
                 bytes / (double)(1LL << 30));
    else if (bytes >= (1LL << 20))
        snprintf(buffer, sizeof(buffer), "%.2f MB", bytes / (double)(1LL << 20));
    else if (bytes >= (1LL << 10))
        snprintf(buffer, sizeof(buffer), "%.2f KB", bytes / (double)(1LL << 10));
    else
        snprintf(buffer, sizeof(buffer), "%.0f B", (double)bytes);
    return buffer;
}


int Utils::get_ngl(int vram_mb) {
    if (vram_mb < 2048) return 0;

    int step = (vram_mb - 2048) / 512;
    return std::min(14 + step * 2, 32);
}


std::optional<Utils::CudaMemoryInfo> Utils::query_cuda_memory() {
    if (auto& probe = cuda_memory_probe()) {
        return probe();
    }
#ifdef _WIN32
    std::string dllName = get_cudart_dll_name();
    LibraryHandle lib = loadLibrary(dllName.c_str());
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

    if (cudaMemGetInfo(&free_bytes, &total_bytes) != 0) {
        log_core(spdlog::level::warn, "Warning: cudaMemGetInfo failed");
        free_bytes = 0;
        total_bytes = 0;
    }

    if (total_bytes == 0 && cudaGetDeviceProperties) {
        // As a fallback, query total memory from cudaGetDeviceProperties.
        constexpr size_t cudaDevicePropSize = 2560;
        alignas(std::max_align_t) uint8_t prop_buffer[cudaDevicePropSize];
        std::memset(prop_buffer, 0, sizeof(prop_buffer));
        if (cudaGetDeviceProperties(prop_buffer, 0) == 0) {
            struct DevicePropShim {
                size_t totalGlobalMem;
            };
            auto *prop = reinterpret_cast<DevicePropShim*>(prop_buffer);
            total_bytes = prop->totalGlobalMem;
        }
    }

    closeLibrary(lib);

    if (free_bytes == 0 && total_bytes == 0) {
        log_core(spdlog::level::warn, "CUDA memory metrics unavailable (both free and total bytes are zero).");
        return std::nullopt;
    }

    CudaMemoryInfo info;
    info.free_bytes = free_bytes;
    info.total_bytes = total_bytes;
    return info;
}


int Utils::compute_ngl_from_cuda_memory(const CudaMemoryInfo& info) {
    size_t usable_bytes = (info.free_bytes > 0) ? info.free_bytes : info.total_bytes;
    if (usable_bytes == 0) {
        return 0;
    }
    int vram_mb = static_cast<int>(usable_bytes / (1024 * 1024));
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
    // Detect MSIX/packaged context and build LocalCache path that is removed on uninstall.
    UINT32 length = 0;
    LONG rc = GetCurrentPackageFamilyName(&length, nullptr);
    if (rc == APPMODEL_ERROR_NO_PACKAGE) {
        return std::nullopt; // not packaged
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

    const wchar_t* localAppData = _wgetenv(L"LOCALAPPDATA");
    if (!localAppData || *localAppData == L'\0') {
        return std::nullopt;
    }

    std::filesystem::path base(localAppData);
    return base / L"Packages" / std::filesystem::path(family) / L"LocalCache" / L"aifilesorter" / L"llms";
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


std::string Utils::get_default_llm_destination()
{
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
    auto last_slash = url.find_last_of('/');
    if (last_slash == std::string::npos || last_slash == url.length() - 1) {
        throw std::runtime_error("Invalid download URL: can't extract filename");
    }
    std::string filename = url.substr(last_slash + 1);

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

} // namespace TestHooks

#ifdef _WIN32
int Utils::get_installed_cuda_runtime_version()
{
    HMODULE hCuda = LoadLibraryA("nvcuda.dll");
    if (!hCuda) {
        log_core(spdlog::level::warn, "Failed to load nvcuda.dll");
        return 0;
    }

    using cudaDriverGetVersion_t = int(__cdecl *)(int*);
    auto cudaDriverGetVersion = reinterpret_cast<cudaDriverGetVersion_t>(
        GetProcAddress(hCuda, "cuDriverGetVersion")
    );

    if (!cudaDriverGetVersion) {
        log_core(spdlog::level::warn, "Failed to get cuDriverGetVersion symbol");
        FreeLibrary(hCuda);
        return 0;
    }

    int version = 0;
    if (cudaDriverGetVersion(&version) != 0) {
        log_core(spdlog::level::warn, "cuDriverGetVersion call failed");
        FreeLibrary(hCuda);
        return 0;
    }

    log_core(spdlog::level::info, "[CUDA] Detected CUDA driver version: {}", version);

    FreeLibrary(hCuda);
    return version;
}
#endif


#ifdef _WIN32
std::string Utils::get_cudart_dll_name() {
    int version = get_installed_cuda_runtime_version();
    std::vector<int> candidates;

    if (version > 0) {
        int suggested = version / 1000;
        if (suggested > 0) {
            candidates.push_back(suggested);
        }
    }

    // probe the most common recent runtimes (highest first)
    for (int v = 15; v >= 10; --v) {
        if (std::find(candidates.begin(), candidates.end(), v) == candidates.end()) {
            candidates.push_back(v);
        }
    }

    for (int major : candidates) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "cudart64_%d.dll", major);
        HMODULE h = LoadLibraryA(buffer);
        if (h) {
            FreeLibrary(h);
            log_core(spdlog::level::info, "[CUDA] Selected runtime DLL: {}", buffer);
            return buffer;
        }
    }

    log_core(spdlog::level::warn, "[CUDA] Unable to locate a compatible cudart64_XX.dll");
    return "";
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
    std::string cleaned;
    cleaned.reserve(value.size());

    // Replace invalid path characters and control chars with spaces.
    for (unsigned char ch : value) {
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
