#include "CurlTls.hpp"

#include <algorithm>
#include <array>
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

#if LIBCURL_VERSION_NUM >= 0x075700
bool supports_sectrust_backend(std::string_view backend)
{
    constexpr std::array prefixes{
        "OpenSSL/", "LibreSSL/", "BoringSSL", "AWS-LC/", "GnuTLS/"};
    return std::any_of(prefixes.begin(), prefixes.end(), [backend](const char* prefix) {
        return backend.starts_with(prefix);
    });
}
#endif

void require_option(CURLcode result)
{
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Failed to configure TLS verification: ") +
                                 curl_easy_strerror(result));
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
    require_option(curl_global_init(CURL_GLOBAL_DEFAULT));
#ifdef __APPLE__
    if (!supports_apple_trust(curl_version_info(CURLVERSION_NOW))) {
        curl_global_cleanup();
        throw std::runtime_error(
            "This macOS libcurl does not support active Apple certificate trust. "
            "Initialize Secure Transport before creating cURL handles, or use "
            "a libcurl build with AppleSecTrust enabled.");
    }
#endif
}

bool CurlTls::supports_apple_trust(const curl_version_info_data* version) noexcept
{
    if (!version || !(version->features & CURL_VERSION_SSL) || !version->ssl_version) {
        return false;
    }
    const auto backend = active_backend(version->ssl_version);
    if (uses_secure_transport(backend)) {
        return true;
    }
#if LIBCURL_VERSION_NUM >= 0x075700 // feature_names was added in curl 7.87.0.
    if (supports_sectrust_backend(backend) && version->age >= CURLVERSION_ELEVENTH &&
        version->feature_names) {
        for (auto feature = version->feature_names; *feature; ++feature) {
            if (std::string_view(*feature) == "AppleSecTrust") {
                return true;
            }
        }
    }
#endif
    return false;
}

void CurlTls::configure(CURL* handle)
{
    if (!handle) {
        throw std::invalid_argument("Cannot configure TLS on a null cURL handle");
    }
#ifndef _WIN32
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L));
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYPEER, 1L));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYHOST, 2L));
#ifdef __APPLE__
    const auto* version = curl_version_info(CURLVERSION_NOW);
    if (!supports_apple_trust(version)) {
        throw std::runtime_error(
            "This macOS libcurl does not support Apple certificate trust. "
            "Use Secure Transport or a libcurl build with AppleSecTrust enabled.");
    }
    // A custom CA file can bypass SecTrust or reintroduce distrusted trust anchors.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO, nullptr));
    require_option(curl_easy_setopt(handle, CURLOPT_CAPATH, nullptr));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, nullptr));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAPATH, nullptr));
#if LIBCURL_VERSION_NUM >= 0x074D00 // CAINFO_BLOB was added in curl 7.77.0.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, nullptr));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO_BLOB, nullptr));
#endif
    const auto backend = active_backend(version->ssl_version);
    if (!uses_secure_transport(backend)) {
        require_option(curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS,
                                       static_cast<long>(CURLSSLOPT_NATIVE_CA)));
        require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_OPTIONS,
                                       static_cast<long>(CURLSSLOPT_NATIVE_CA)));
    }
#endif
#endif
}
