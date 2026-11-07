#pragma once

#include <curl/curl.h>

/** @brief Configure HTTPS verification using operating-system trust on POSIX hosts. */
namespace CurlTls {

/**
 * @brief Initialize libcurl, selecting native Apple TLS before any easy handle exists.
 * @throws std::runtime_error If initialization or Apple native verification is unavailable.
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
 * @brief Enable certificate and hostname verification using the platform trust policy.
 *
 * Linux retains libcurl's distribution-configured trust locations. macOS requires
 * Secure Transport and clears file/blob trust overrides. Windows callers
 * keep their existing, separately configured certificate-bundle policy.
 * @param handle Initialized libcurl easy handle.
 * @pre On macOS call initialize() before creating easy handles.
 * @throws std::invalid_argument If handle is null.
 * @throws std::runtime_error If native Apple trust is unavailable or configuration fails.
 */
void configure(CURL* handle);

} // namespace CurlTls
