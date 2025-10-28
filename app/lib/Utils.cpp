#include "Utils.hpp"
#include "Logger.hpp"
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
}
#ifdef _WIN32
    #include <windows.h>
    #include <wininet.h>
#elif __linux__
    #include <dlfcn.h>
    #include <limits.h>
    #include <unistd.h>
#elif __APPLE__
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
    #include <limits.h>
#endif
#include <iostream>
#include <Types.hpp>
#include <cstddef>

// For OpenCL dynamic library loading
typedef unsigned int cl_uint;
typedef int cl_int;
typedef size_t cl_device_type;
typedef struct _cl_platform_id* cl_platform_id;
typedef struct _cl_device_id* cl_device_id;

// These constants are normally defined in cl.h
constexpr cl_int CL_SUCCESS = 0;
constexpr cl_device_type CL_DEVICE_TYPE_ALL = 0xFFFFFFFF;
constexpr cl_uint CL_DEVICE_NAME = 0x102B;


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
    int result = system("ping -c 1 google.com > /dev/null 2>&1");
    return result == 0;
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


bool Utils::is_valid_directory(const char *path)
{
    if (!path || *path == '\0') {
        return false;
    }
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(path), ec);
}


std::vector<unsigned char> Utils::hex_to_vector(const std::string& hex) {
    std::vector<unsigned char> data;
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned char byte = static_cast<unsigned char>(
            std::stoi(hex.substr(i, 2), nullptr, 16));
        data.push_back(byte);
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
        snprintf(buffer, sizeof(buffer), "%.2f B", bytes / (double)(1LL << 10));
    return buffer;
}


int Utils::get_ngl(int vram_mb) {
    if (vram_mb < 2048) return 0;

    int step = (vram_mb - 2048) / 512;
    return std::min(14 + step * 2, 32);
}


std::optional<Utils::CudaMemoryInfo> Utils::query_cuda_memory() {
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
        return (std::filesystem::path(appdata) / "aifilesorter" / "llms").string();
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


bool Utils::is_cuda_available() {
    log_core(spdlog::level::info, "[CUDA] Checking CUDA availability...");

#ifdef _WIN32
    std::string dllName = get_cudart_dll_name();

    if (dllName.empty()) {
        log_core(spdlog::level::warn, "[CUDA] DLL name is empty — likely failed to get CUDA version.");
        return false;
    }

    LibraryHandle handle = loadLibrary(dllName.c_str());
    log_core(spdlog::level::info, "[CUDA] Trying to load: {} => {}", dllName, handle ? "Success" : "Failure");
#else
    LibraryHandle handle = loadLibrary("libcudart.so");
#endif

    if (!handle) {
        log_core(spdlog::level::warn, "[CUDA] Failed to load CUDA runtime library.");
        return false;
    }

    typedef int (*cudaGetDeviceCount_t)(int*);
    typedef int (*cudaSetDevice_t)(int);
    typedef int (*cudaMemGetInfo_t)(size_t*, size_t*);

    auto cudaGetDeviceCount = reinterpret_cast<cudaGetDeviceCount_t>(
        getSymbol(handle, "cudaGetDeviceCount"));
    auto cudaSetDevice = reinterpret_cast<cudaSetDevice_t>(
        getSymbol(handle, "cudaSetDevice"));
    auto cudaMemGetInfo = reinterpret_cast<cudaMemGetInfo_t>(
        getSymbol(handle, "cudaMemGetInfo"));

    log_core(spdlog::level::info, "[CUDA] Lookup cudaGetDeviceCount symbol: {}",
             cudaGetDeviceCount ? "Found" : "Not Found");

    if (!cudaGetDeviceCount || !cudaSetDevice || !cudaMemGetInfo) {
        closeLibrary(handle);
        return false;
    }

    int count = 0;
    int status = cudaGetDeviceCount(&count);
    log_core(spdlog::level::info, "[CUDA] cudaGetDeviceCount returned status: {}, device count: {}", status, count);

    if (status != 0) {
        log_core(spdlog::level::warn, "[CUDA] CUDA error: {} from cudaGetDeviceCount", status);
        closeLibrary(handle);
        return false;
    }
    if (count == 0) {
        log_core(spdlog::level::warn, "[CUDA] No CUDA devices found");
        closeLibrary(handle);
        return false;
    }

    int set_status = cudaSetDevice(0);
    if (set_status != 0) {
        log_core(spdlog::level::warn, "[CUDA] Failed to set CUDA device 0 (error {})", set_status);
        closeLibrary(handle);
        return false;
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    int mem_status = cudaMemGetInfo(&free_bytes, &total_bytes);
    if (mem_status != 0) {
        log_core(spdlog::level::warn, "[CUDA] cudaMemGetInfo failed (error {})", mem_status);
        closeLibrary(handle);
        return false;
    }

    log_core(spdlog::level::info, "[CUDA] CUDA is available and {} device(s) found.", count);
    closeLibrary(handle);
    return true;
}


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

    std::filesystem::path fs_path(path);
    std::string generic_path = fs_path.generic_string();

    std::vector<std::string> prefixes;

    if (const char* home = std::getenv("HOME")) {
        std::filesystem::path home_path(home);
        prefixes.push_back(home_path.generic_string());
    }

    if (const char* userprofile = std::getenv("USERPROFILE")) {
        std::filesystem::path profile_path(userprofile);
        prefixes.push_back(profile_path.generic_string());
    }

    // Add common Windows-style prefix if not already covered
    if (prefixes.empty() && Utils::is_os_windows()) {
        if (const char* username = std::getenv("USERNAME")) {
            std::string win_prefix = std::string("C:/Users/") + username;
            prefixes.push_back(win_prefix);
        }
    }

    for (auto prefix : prefixes) {
        if (prefix.empty()) {
            continue;
        }
        if (prefix.back() != '/') {
            prefix += '/';
        }

        if (generic_path.size() >= prefix.size()) {
            if (std::equal(prefix.begin(), prefix.end(), generic_path.begin())) {
                std::string trimmed = generic_path.substr(prefix.size());
                while (!trimmed.empty() && trimmed.front() == '/') {
                    trimmed.erase(trimmed.begin());
                }
                if (!trimmed.empty()) {
                    return trimmed;
                }
            }
        }
    }

    // If no prefix matched or trimming resulted in empty string, fall back to removing leading separator.
    std::string sanitized = generic_path;
    while (!sanitized.empty() && (sanitized.front() == '/' || sanitized.front() == '\\')) {
        sanitized.erase(sanitized.begin());
    }

    return sanitized.empty() ? fs_path.filename().generic_string() : sanitized;
}


bool Utils::is_opencl_available(std::vector<std::string>* device_names)
{
#ifdef _WIN32
    LibraryHandle handle = loadLibrary("OpenCL.dll");
#else
    LibraryHandle handle = loadLibrary("libOpenCL.so");
#endif

    if (!handle) return false;

    using clGetPlatformIDs_t = cl_int (*)(cl_uint, cl_platform_id*, cl_uint*);
    using clGetDeviceIDs_t = cl_int (*)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
    using clGetDeviceInfo_t = cl_int (*)(cl_device_id, cl_uint, size_t, void*, size_t*);

    auto clGetPlatformIDs = reinterpret_cast<clGetPlatformIDs_t>(getSymbol(handle, "clGetPlatformIDs"));
    auto clGetDeviceIDs = reinterpret_cast<clGetDeviceIDs_t>(getSymbol(handle, "clGetDeviceIDs"));
    auto clGetDeviceInfo = reinterpret_cast<clGetDeviceInfo_t>(getSymbol(handle, "clGetDeviceInfo"));

    if (!clGetPlatformIDs || !clGetDeviceIDs || !clGetDeviceInfo) {
        closeLibrary(handle);
        return false;
    }

    cl_platform_id platform;
    cl_uint num_platforms = 0;
    if (clGetPlatformIDs(1, &platform, &num_platforms) != CL_SUCCESS || num_platforms == 0) {
        closeLibrary(handle);
        return false;
    }

    cl_device_id devices[4];
    cl_uint num_devices = 0;
    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 4, devices,
                       &num_devices) !=CL_SUCCESS || num_devices == 0) {
        closeLibrary(handle);
        return false;
    }

    if (device_names) {
        for (cl_uint i = 0; i < num_devices; ++i) {
            char name[256];
            if (clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(name),
                name, nullptr) == CL_SUCCESS) {
                device_names->emplace_back(name);
            }
        }
    }

    closeLibrary(handle);
    return true;
}
