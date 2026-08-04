#ifndef REMOTE_API_ERROR_HPP
#define REMOTE_API_ERROR_HPP

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace spdlog {
class logger;
}

namespace RemoteApiError {

/**
 * @brief Parses a Retry-After header value expressed as seconds.
 * @param value Header value to parse.
 * @return Retry delay in seconds, or std::nullopt when the value is unsupported.
 */
std::optional<int> parse_retry_after_seconds(std::string_view value);

/**
 * @brief Extracts a concise error message from a remote API response body.
 * @param payload HTTP response body, JSON or plain text.
 * @return Provider error message or a trimmed response-body excerpt.
 */
std::string extract_error_message(std::string_view payload);

/**
 * @brief Throws the appropriate exception for a non-success remote HTTP response.
 * @param provider_name Human-readable provider name for diagnostics.
 * @param http_code HTTP status code.
 * @param payload HTTP response body.
 * @param retry_after_header Retry-After response header value, if any.
 * @param logger Optional logger for diagnostic details.
 */
[[noreturn]] void throw_for_http_error(std::string_view provider_name,
                                       long http_code,
                                       std::string_view payload,
                                       std::string_view retry_after_header,
                                       const std::shared_ptr<spdlog::logger>& logger);

} // namespace RemoteApiError

#endif // REMOTE_API_ERROR_HPP
