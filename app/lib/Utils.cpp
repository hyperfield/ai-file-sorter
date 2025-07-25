#include "Utils.hpp"
#include <cstring>  // for memset
#include <filesystem>
#include <stdlib.h>
#include <string>
#include <glibmm/fileutils.h>
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
    GetModuleFileName(NULL, result, MAX_PATH);
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


bool Utils::is_valid_directory(const char *path)
{
    return g_file_test(path, G_FILE_TEST_IS_DIR);
}


std::vector<unsigned char> Utils::hex_to_vector(const std::string& hex) {
    std::vector<unsigned char> data;
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned char byte = static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16));
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
        std::cerr << "Error creating log directory: " << e.what() << std::endl;
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
        snprintf(buffer, sizeof(buffer), "%.2f GB", bytes / (double)(1LL << 30));
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


int Utils::determine_ngl_cuda() {
#ifdef _WIN32
    std::string dllName = get_cudart_dll_name();
    LibraryHandle lib = loadLibrary(dllName.c_str());
#else
    const char* dllName = "libcudart.so";
    LibraryHandle lib = loadLibrary(dllName);
#endif

    if (!lib) {
        std::cerr << "Failed to load CUDA runtime library: " << dllName << "\n";
        return 0;
    }

    using cudaGetDeviceProperties_t = int (*)(void*, int);
    using cudaMemGetInfo_t = int (*)(size_t*, size_t*);

    auto cudaGetDeviceProperties = reinterpret_cast<cudaGetDeviceProperties_t>(
        getSymbol(lib, "cudaGetDeviceProperties"));
    auto cudaMemGetInfo = reinterpret_cast<cudaMemGetInfo_t>(
        getSymbol(lib, "cudaMemGetInfo"));

    if (!cudaGetDeviceProperties) {
        std::cerr << "Failed to load symbol: cudaGetDeviceProperties\n";
        closeLibrary(lib);
        return 0;
    }

    constexpr size_t cudaDevicePropSize = 2560;
    alignas(std::max_align_t) uint8_t prop_buffer[cudaDevicePropSize];
    std::memset(prop_buffer, 0, sizeof(prop_buffer));

    int device = 0;
    if (cudaGetDeviceProperties(prop_buffer, device) != 0) {
        std::cerr << "Warning: cudaGetDeviceProperties failed\n";
        closeLibrary(lib);
        return 0;
    }

    size_t total_mem_bytes = *reinterpret_cast<size_t*>(prop_buffer);
    size_t free_bytes = 0;

    if (cudaMemGetInfo) {
        if (cudaMemGetInfo(&free_bytes, &total_mem_bytes) != 0) {
            std::cerr << "Warning: cudaMemGetInfo failed\n";
            free_bytes = 0;
        }
    }

    closeLibrary(lib);

    size_t usable_bytes = free_bytes > 0 ? free_bytes : total_mem_bytes;
    int vram_mb = static_cast<int>(usable_bytes / (1024 * 1024));

    return get_ngl(vram_mb);
}


template <typename Func>
void Utils::run_on_main_thread(Func&& func)
{
    auto* task = new std::function<void()>(std::forward<Func>(func));
    g_idle_add([](gpointer data) -> gboolean {
        auto* f = static_cast<std::function<void()>*>(data);
        (*f)();
        delete f;
        return G_SOURCE_REMOVE;
    }, task);
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
    std::cerr << "[CUDA] Checking CUDA availability..." << std::endl;

#ifdef _WIN32
    std::string dllName = get_cudart_dll_name();

    if (dllName.empty()) {
        std::cerr << "[CUDA] DLL name is empty — likely failed to get CUDA version." << std::endl;
        return false;
    }

    LibraryHandle handle = loadLibrary(dllName.c_str());
    std::cerr << "[CUDA] Trying to load: " << dllName << " => " << (handle ? "Success" : "Failure") << std::endl;
#else
    LibraryHandle handle = loadLibrary("libcudart.so");
#endif

    if (!handle) {
        std::cerr << "[CUDA] Failed to load CUDA runtime library." << std::endl;
        return false;
    }

    typedef int (*cudaGetDeviceCount_t)(int*);
    auto cudaGetDeviceCount = reinterpret_cast<cudaGetDeviceCount_t>(
                                    getSymbol(handle, "cudaGetDeviceCount"));
    std::cerr << "[CUDA] Lookup cudaGetDeviceCount symbol: " << (
        cudaGetDeviceCount ? "Found" : "Not Found") << std::endl;

    if (!cudaGetDeviceCount) {
        closeLibrary(handle);
        return false;
    }

    int count = 0;
    int status = cudaGetDeviceCount(&count);
    std::cerr << "[CUDA] cudaGetDeviceCount returned status: " << status << ", device count: " << count << std::endl;

    if (status != 0) {
        std::cerr << "[CUDA] CUDA error: " << status << " from cudaGetDeviceCount\n";
        closeLibrary(handle);
        return false;
    }
    if (count == 0) {
        std::cerr << "[CUDA] No CUDA devices found\n";
        closeLibrary(handle);
        return false;
    }

    std::cerr << "[CUDA] CUDA is available and " << count << " device(s) found." << std::endl;
    closeLibrary(handle);
    return true;
}


#ifdef _WIN32
int Utils::get_installed_cuda_runtime_version()
{
    HMODULE hCuda = LoadLibraryA("nvcuda.dll");
    if (!hCuda) {
        std::cerr << "Failed to load nvcuda.dll\n";
        return 0;
    }

    using cudaDriverGetVersion_t = int(__cdecl *)(int*);
    auto cudaDriverGetVersion = reinterpret_cast<cudaDriverGetVersion_t>(
        GetProcAddress(hCuda, "cuDriverGetVersion")
    );

    if (!cudaDriverGetVersion) {
        std::cerr << "Failed to get cuDriverGetVersion symbol\n";
        FreeLibrary(hCuda);
        return 0;
    }

    int version = 0;
    if (cudaDriverGetVersion(&version) != 0) {
        std::cerr << "cuDriverGetVersion call failed\n";
        FreeLibrary(hCuda);
        return 0;
    }

    std::cerr << "[CUDA] Detected CUDA driver version: " << version << std::endl;

    FreeLibrary(hCuda);
    return version;
}
#endif


#ifdef _WIN32
std::string Utils::get_cudart_dll_name() {
    int version = get_installed_cuda_runtime_version();
    if (version == 0) return "";

    int major = version / 1000;        // e.g., 12
    // int minor = (version % 1000) / 10; // e.g., 8

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "cudart64_%d.dll", major);
    std::cerr << "[CUDA] Determined DLL name: " << buffer << std::endl;
    return buffer; // e.g., "cudart64_12.dll"
}
#endif


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
