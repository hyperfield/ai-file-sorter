#pragma once

#include <curl/curl.h>
#include <string>

/** @brief Configure HTTPS verification using maintained platform certificate trust. */
namespace CurlTls {

/**
 * @brief Initialize libcurl, selecting native Windows/Apple TLS before any easy handle exists.
 * @throws std::runtime_error If initialization or the required native backend is unavailable.
 * @note The caller must balance a successful call with curl_global_cleanup().
 */
void initialize();

/**
 * @brief Check whether a libcurl runtime enforces exclusive Apple certificate trust.
 * @param version Runtime information returned by curl_version_info().
 * @return True only for active Secure Transport. AppleSecTrust alone is additive.
 */
bool supports_apple_trust(const curl_version_info_data* version) noexcept;

/**
 * @brief Check whether the active runtime supports native Windows trust and revocation.
 * @param version Runtime information returned by curl_version_info().
 * @return True only for active Schannel, excluding inactive MultiSSL entries.
 */
bool supports_windows_trust(const curl_version_info_data* version) noexcept;

/**
 * @brief Describe the active libcurl runtime and its loaded DLL path on Windows.
 * @return Version/backend description, including a Windows DLL path when available.
 */
std::string runtime_description();

/**
 * @brief Enable certificate and hostname verification using the platform trust policy.
 *
 * Linux retains libcurl's distribution-configured trust locations. macOS requires
 * Secure Transport; Windows requires Schannel with strict revocation. Both native
 * policies clear origin/proxy file/blob trust overrides without a PEM fallback.
 * @param handle Initialized libcurl easy handle.
 * @pre Call initialize() before creating easy handles.
 * @throws std::invalid_argument If handle is null.
 * @throws std::runtime_error If the required native backend is unavailable or configuration fails.
 */
void configure(CURL* handle);

} // namespace CurlTls
