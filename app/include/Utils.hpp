#ifndef UTILS_HPP
#define UTILS_HPP

#include <curl/system.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>


class Utils {
public:
    Utils();
    ~Utils();
    static bool is_network_available();
    static std::string get_executable_path();
    static bool is_valid_directory(const char *path);
    static std::vector<unsigned char> hex_to_vector(const std::string &hex);
    static const char* to_cstr(const std::u8string& u8str);
    static void ensure_directory_exists(const std::string &dir);
    static bool is_os_windows();
    static bool is_os_macos();
    static bool is_os_linux();
    static std::string format_size(curl_off_t bytes);
    static int determine_ngl_cuda();
    struct CudaMemoryInfo {
        size_t free_bytes{0};
        size_t total_bytes{0};

        bool valid() const {
            return total_bytes > 0 || free_bytes > 0;
        }
    };
    static std::optional<CudaMemoryInfo> query_cuda_memory();
    static int compute_ngl_from_cuda_memory(const CudaMemoryInfo& info);
    template <typename Func> void run_on_main_thread(Func &&func);
    static std::string get_default_llm_destination();
    static std::string get_file_name_from_url(std::string url);
    static std::string make_default_path_to_file_from_download_url(std::string url);
    static bool is_cuda_available();
    static int get_installed_cuda_runtime_version();
    static std::string get_cudart_dll_name();
    static std::string abbreviate_user_path(const std::string& path);
    static std::filesystem::path ensure_ca_bundle();
    static std::string path_to_utf8(const std::filesystem::path& path);
    static std::filesystem::path utf8_to_path(const std::string& utf8_path);

private:
    static int get_ngl(int vram_mb);
};

#endif
