#include "RemoteApiError.hpp"

#include "LLMErrors.hpp"

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#else
#error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

constexpr size_t kMaxErrorExcerptLength = 240;

std::string trim_copy(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string collapse_spaces_copy(std::string value)
{
    std::string collapsed;
    collapsed.reserve(value.size());
    bool previous_space = false;
    for (unsigned char ch : value) {
        if (std::isspace(ch)) {
            if (!previous_space) {
                collapsed.push_back(' ');
            }
            previous_space = true;
            continue;
        }
        collapsed.push_back(static_cast<char>(ch));
        previous_space = false;
    }
    return trim_copy(std::move(collapsed));
}

std::string truncate_copy(std::string value)
{
    if (value.size() <= kMaxErrorExcerptLength) {
        return value;
    }
    value.resize(kMaxErrorExcerptLength);
    return value + "...";
}

std::optional<Json::Value> parse_json(std::string_view payload)
{
    if (payload.empty()) {
        return std::nullopt;
    }

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    const char* begin = payload.data();
    const char* end = begin + payload.size();
    if (!reader->parse(begin, end, &root, &errors)) {
        return std::nullopt;
    }
    return root;
}

std::string string_field(const Json::Value& value, const char* key)
{
    const auto& field = value[key];
    return field.isString() ? field.asString() : std::string();
}

std::string extract_error_status(const Json::Value& root)
{
    const auto& error = root["error"];
    if (error.isObject()) {
        return string_field(error, "status");
    }
    return string_field(root, "status");
}

std::optional<int> clamp_retry_seconds(double seconds)
{
    if (seconds <= 0.0) {
        return std::nullopt;
    }
    constexpr double max_int = static_cast<double>(std::numeric_limits<int>::max());
    if (seconds > max_int) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(std::ceil(seconds));
}

std::optional<int> parse_retry_seconds_text(std::string_view text)
{
    const std::string input(text);
    std::smatch match;
    const std::regex retry_regex(R"(retry\s+in\s+([0-9]+(?:\.[0-9]+)?)s)", std::regex::icase);
    if (std::regex_search(input, match, retry_regex) && match.size() > 1) {
        try {
            return clamp_retry_seconds(std::stod(match[1].str()));
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<int> retry_seconds_from_value(const Json::Value& value)
{
    if (value.isNumeric()) {
        return clamp_retry_seconds(value.asDouble());
    }
    if (value.isString()) {
        if (auto retry = RemoteApiError::parse_retry_after_seconds(value.asString())) {
            return retry;
        }
        return parse_retry_seconds_text(value.asString());
    }
    return std::nullopt;
}

std::optional<int> extract_retry_after_from_json(const Json::Value& root)
{
    const std::initializer_list<const char*> top_level_keys = {
        "retry_after",
        "retry_after_seconds",
        "retryAfter",
        "retryAfterSeconds"
    };
    for (const char* key : top_level_keys) {
        if (auto retry = retry_seconds_from_value(root[key])) {
            return retry;
        }
    }

    const auto& error = root["error"];
    if (error.isObject()) {
        for (const char* key : top_level_keys) {
            if (auto retry = retry_seconds_from_value(error[key])) {
                return retry;
            }
        }
        if (auto retry = parse_retry_seconds_text(string_field(error, "message"))) {
            return retry;
        }
        const auto& details = error["details"];
        if (details.isArray()) {
            for (const auto& detail : details) {
                if (auto retry = retry_seconds_from_value(detail["retryDelay"])) {
                    return retry;
                }
            }
        }
    }

    if (auto retry = parse_retry_seconds_text(string_field(root, "message"))) {
        return retry;
    }
    return std::nullopt;
}

std::string build_http_error_prefix(long http_code)
{
    if (http_code == 401) {
        return "Authentication Error";
    }
    if (http_code == 403) {
        return "Authorization Error";
    }
    if (http_code >= 500) {
        return "Server Error";
    }
    return "Client Error";
}

} // namespace

namespace RemoteApiError {

std::optional<int> parse_retry_after_seconds(std::string_view value)
{
    std::string trimmed(value);
    trimmed = trim_copy(std::move(trimmed));
    if (trimmed.empty()) {
        return std::nullopt;
    }

    char* end = nullptr;
    const double parsed = std::strtod(trimmed.c_str(), &end);
    if (end != trimmed.c_str() && *end == '\0') {
        return clamp_retry_seconds(parsed);
    }
    return std::nullopt;
}

std::string extract_error_message(std::string_view payload)
{
    if (auto root = parse_json(payload)) {
        const auto& error = (*root)["error"];
        if (error.isObject()) {
            const std::string status = string_field(error, "status");
            const std::string message = string_field(error, "message");
            if (!status.empty() && !message.empty()) {
                return status + ": " + message;
            }
            if (!message.empty()) {
                return message;
            }
            if (!status.empty()) {
                return status;
            }
        } else if (error.isString()) {
            return error.asString();
        }

        const std::string message = string_field(*root, "message");
        if (!message.empty()) {
            return message;
        }
    }

    return truncate_copy(collapse_spaces_copy(std::string(payload)));
}

[[noreturn]] void throw_for_http_error(std::string_view provider_name,
                                       long http_code,
                                       std::string_view payload,
                                       std::string_view retry_after_header,
                                       const std::shared_ptr<spdlog::logger>& logger)
{
    const std::string provider(provider_name);
    const std::string message = extract_error_message(payload);
    const auto root = parse_json(payload);
    const std::string status = root ? extract_error_status(*root) : std::string();
    std::optional<int> retry_after = parse_retry_after_seconds(retry_after_header);
    if (!retry_after && root) {
        retry_after = extract_retry_after_from_json(*root);
    }

    if (http_code == 429 || status == "RESOURCE_EXHAUSTED") {
        if (logger) {
            logger->warn("{} rate limit response (HTTP {}): {}", provider, http_code, message);
        }
        const std::string detail = message.empty()
            ? provider + " rate limit reached."
            : provider + " rate limit reached: " + message;
        throw BackoffError(detail, retry_after.value_or(0));
    }

    if (logger) {
        logger->error("{} HTTP {} error: {}", provider, http_code, message);
    }

    std::ostringstream oss;
    oss << build_http_error_prefix(http_code) << ": " << provider << " returned HTTP " << http_code;
    if (!message.empty()) {
        oss << ": " << message;
    }
    throw std::runtime_error(oss.str());
}

} // namespace RemoteApiError
