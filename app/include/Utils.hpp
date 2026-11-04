#ifndef UTILS_HPP
#define UTILS_HPP

#include <curl/system.h>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
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
        size_t device_total_bytes{0};

        bool valid() const {
            return total_bytes > 0 || free_bytes > 0;
        }
    };
    static std::optional<CudaMemoryInfo> query_cuda_memory();
    static int compute_ngl_from_cuda_memory(const CudaMemoryInfo& info);
    template <typename Func> void run_on_main_thread(Func &&func);
    /**
     * @brief Set the process-local override for downloaded local LLM storage.
     * @param path Directory to use; empty clears the override and falls back to defaults.
     */
    static void set_llm_storage_directory_override(const std::string& path);
    /**
     * @brief Return the process-local local LLM storage override.
     * @return Override directory, or empty when platform defaults are used.
     */
    static std::string get_llm_storage_directory_override();
    /**
     * @brief Return the effective directory used for downloaded local LLM files.
     * @return User override, environment override, or platform-specific default directory.
     */
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
    /**
     * @brief Read an environment variable as UTF-8 regardless of the process ANSI code page.
     * @param name ASCII variable name.
     * @return The UTF-8 value, or std::nullopt when the variable is unset or empty.
     *
     * On Windows std::getenv returns text in the ANSI code page (e.g. CP1251), which breaks
     * non-ASCII user profile paths such as C:\Users\Иван. This reads the wide value instead.
     */
    static std::optional<std::string> getenv_utf8(const char* name);
    /**
     * @brief Count Unicode code points in a UTF-8 string (not bytes).
     *
     * Use for user-visible length limits: a Cyrillic letter is 2 bytes in UTF-8.
     */
    static std::size_t utf8_length(std::string_view utf8);
    /**
     * @brief Shrink a UTF-8 string to at most @p max_bytes without splitting a character.
     *
     * Also drops an incomplete multi-byte sequence left at the end (e.g. after reading a
     * fixed-size block from a file), so Cyrillic/CJK text never ends in broken bytes.
     */
    static void utf8_truncate(std::string& utf8, std::size_t max_bytes);
    /**
     * @brief Return the index of the first UTF-8 character boundary at or after @p pos.
     */
    static std::size_t utf8_boundary_at_or_after(std::string_view utf8, std::size_t pos);
    static std::string sanitize_path_label(const std::string& value);

private:
    static int get_ngl(int vram_mb);
};

#endif
