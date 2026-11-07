#include "CurlTls.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

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
    if (selected != CURLSSLSET_OK && selected != CURLSSLSET_UNKNOWN_BACKEND &&
        selected != CURLSSLSET_TOO_LATE) {
        throw std::runtime_error("Unable to select a macOS TLS backend");
    }
#endif
    require_option(curl_global_init(CURL_GLOBAL_DEFAULT), "curl_global_init");
#ifdef __APPLE__
    if (!supports_apple_trust(curl_version_info(CURLVERSION_NOW))) {
        curl_global_cleanup();
        throw std::runtime_error(kNativeTrustRequired);
    }
#endif
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
#ifndef _WIN32
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L), "SSL_VERIFYPEER");
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L), "SSL_VERIFYHOST");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYPEER, 1L), "PROXY_SSL_VERIFYPEER");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYHOST, 2L), "PROXY_SSL_VERIFYHOST");
#ifdef __APPLE__
    const auto* version = curl_version_info(CURLVERSION_NOW);
    if (!supports_apple_trust(version)) {
        throw std::runtime_error(kNativeTrustRequired);
    }
    // A custom CA file can bypass SecTrust or reintroduce distrusted trust anchors.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO, nullptr), "CAINFO");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, nullptr), "PROXY_CAINFO");
    // Secure Transport cannot use CAPATH/PROXY_CAPATH, even with a null value:
    // setting them returns CURLE_NOT_BUILT_IN on Apple's system libcurl.
#if LIBCURL_VERSION_NUM >= 0x074D00 // CAINFO_BLOB was added in curl 7.77.0.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, nullptr), "CAINFO_BLOB");
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO_BLOB, nullptr), "PROXY_CAINFO_BLOB");
#endif
#endif
#endif
}
