#include "CurlTls.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
std::string_view active_backend(std::string_view versions)
{
    // MultiSSL lists inactive backends in parentheses, sometimes before the active one.
    while (!versions.empty()) {
        if (versions.front() == ' ') {
            versions.remove_prefix(1);
        } else if (versions.front() == '(') {
            const auto end = versions.find(')');
            if (end == std::string_view::npos) {
                return {};
            }
            versions.remove_prefix(end + 1);
        } else {
            return versions.substr(0, versions.find(' '));
        }
    }
    return {};
}

bool uses_secure_transport(std::string_view backend)
{
    return backend == "SecureTransport";
}

#ifdef __APPLE__
constexpr const char* kNativeTrustRequired =
    "This macOS libcurl requires active Secure Transport for exclusive Apple certificate trust. "
    "Use the macOS SDK/system libcurl and initialize it before creating cURL handles. "
    "AppleSecTrust alone is insufficient: other backends can supplement it with OpenSSL CA anchors.";
#elif defined(_WIN32)
constexpr const char* kNativeTrustRequired =
    "This Windows libcurl requires active Schannel for native certificate trust and strict revocation. "
    "Use a Schannel-enabled libcurl and initialize it before creating cURL handles. "
    "No bundled-CA or alternate-backend fallback is supported.";
#endif

void require_option(CURLcode result, const char* operation)
{
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Failed to configure TLS verification (") +
                                 operation + "): " + curl_easy_strerror(result));
    }
}
} // namespace

void CurlTls::initialize()
{
#ifdef __APPLE__
    // Apple's system libcurl normally defaults to LibreSSL even when Secure
    // Transport is available. Selection must happen before curl_global_init().
    const auto selected = curl_global_sslset(CURLSSLBACKEND_SECURETRANSPORT, nullptr, nullptr);
#elif defined(_WIN32)
    const auto selected = curl_global_sslset(CURLSSLBACKEND_SCHANNEL, nullptr, nullptr);
#endif
#if defined(__APPLE__) || defined(_WIN32)
    if (selected != CURLSSLSET_OK && selected != CURLSSLSET_UNKNOWN_BACKEND &&
        selected != CURLSSLSET_TOO_LATE) {
        throw std::runtime_error("Unable to select the native TLS backend");
    }
#endif
    require_option(curl_global_init(CURL_GLOBAL_DEFAULT), "curl_global_init");
#ifdef __APPLE__
    if (!supports_apple_trust(curl_version_info(CURLVERSION_NOW))) {
#elif defined(_WIN32)
    if (!supports_windows_trust(curl_version_info(CURLVERSION_NOW))) {
#endif
#if defined(__APPLE__) || defined(_WIN32)
        curl_global_cleanup();
        throw std::runtime_error(kNativeTrustRequired);
    }
#endif
}

bool CurlTls::supports_windows_trust(const curl_version_info_data* version) noexcept
{
    return version && (version->features & CURL_VERSION_SSL) && version->ssl_version &&
           active_backend(version->ssl_version) == "Schannel";
}

std::string CurlTls::runtime_description()
{
    std::string description = curl_version();
#ifdef _WIN32
    constexpr DWORD kModulePathCapacity = 32768;
    HMODULE module = nullptr;
    wchar_t path[kModulePathCapacity]{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&curl_version_info), &module) ||
        !GetModuleFileNameW(module, path, kModulePathCapacity)) {
        return description + "; DLL path unavailable";
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return description + "; DLL path unavailable";
    }
    std::string encoded(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, path, -1, encoded.data(), size, nullptr, nullptr);
    encoded.pop_back();
    description += "; DLL: " + encoded;
#endif
    return description;
}

bool CurlTls::supports_apple_trust(const curl_version_info_data* version) noexcept
{
    if (!version || !(version->features & CURL_VERSION_SSL) || !version->ssl_version) {
        return false;
    }
    // CURLSSLOPT_NATIVE_CA is additive. AppleSecTrust support does not prove
    // that an OpenSSL build cannot accept a chain using other trust anchors.
    return uses_secure_transport(active_backend(version->ssl_version));
}

void CurlTls::configure(CURL* handle)
{
    if (!handle) {
        throw std::invalid_argument("Cannot configure TLS on a null cURL handle");
    }
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L), "SSL_VERIFYPEER");
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L), "SSL_VERIFYHOST");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYPEER, 1L), "PROXY_SSL_VERIFYPEER");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYHOST, 2L), "PROXY_SSL_VERIFYHOST");
#ifdef __APPLE__
    const auto* version = curl_version_info(CURLVERSION_NOW);
    if (!supports_apple_trust(version)) {
        throw std::runtime_error(kNativeTrustRequired);
    }
#elif defined(_WIN32)
    if (!supports_windows_trust(curl_version_info(CURLVERSION_NOW))) {
        throw std::runtime_error(kNativeTrustRequired);
    }
    // Zero excludes NO_REVOKE and REVOKE_BEST_EFFORT, including on reused handles.
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, 0L), "SSL_OPTIONS");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_OPTIONS, 0L), "PROXY_SSL_OPTIONS");
#endif
#if defined(__APPLE__) || defined(_WIN32)
    // Custom anchors can bypass native trust or reintroduce distrusted roots.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO, nullptr), "CAINFO");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, nullptr), "PROXY_CAINFO");
    // Secure Transport and Schannel reject CAPATH/PROXY_CAPATH assignments even
    // with null values (CURLE_NOT_BUILT_IN). They cannot add directory anchors.
#if LIBCURL_VERSION_NUM >= 0x074D00 // CAINFO_BLOB was added in curl 7.77.0.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, nullptr), "CAINFO_BLOB");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO_BLOB, nullptr), "PROXY_CAINFO_BLOB");
#endif
#endif
}
