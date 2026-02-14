#include "LLMClient.hpp"
#include "Types.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <filesystem>
#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <utility>

// Helper function to write the response from curl into a string
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response)
{
    size_t totalSize = size * nmemb;
    response->append(static_cast<const char*>(contents), totalSize);
    return totalSize;
}

namespace {
std::string trim_ws(const std::string& value);

std::string escape_json(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                out += c;
        }
    }
    return out;
}

long resolve_custom_timeout_seconds() {
    const char* env = std::getenv("AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT");
    if (env && *env) {
        char* end = nullptr;
        const long value = std::strtol(env, &end, 10);
        if (end != env && value > 0) {
            return value;
        }
    }
    return 60L;
}

long resolve_openai_timeout_seconds() {
    return 5L;
}

long resolve_timeout_seconds(const std::string& base_url) {
    const std::string trimmed = trim_ws(base_url);
    if (trimmed.empty()) {
        return resolve_openai_timeout_seconds();
    }
    return resolve_custom_timeout_seconds();
}

std::string trim_ws(const std::string& value) {
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    const auto end = value.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) {
        return std::string();
    }
    return value.substr(start, end - start + 1);
}

std::string trim_trailing_slashes(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

bool ends_with(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

struct CurlRequest {
    CURL* handle{nullptr};
    curl_slist* headers{nullptr};

    CurlRequest() = default;
    CurlRequest(const CurlRequest&) = delete;
    CurlRequest& operator=(const CurlRequest&) = delete;

    CurlRequest(CurlRequest&& other) noexcept
        : handle(other.handle),
          headers(other.headers)
    {
        other.handle = nullptr;
        other.headers = nullptr;
    }

    CurlRequest& operator=(CurlRequest&& other) noexcept
    {
        if (this != &other) {
            cleanup();
            handle = other.handle;
            headers = other.headers;
            other.handle = nullptr;
            other.headers = nullptr;
        }
        return *this;
    }

    ~CurlRequest() {
        cleanup();
    }

private:
    void cleanup()
    {
        if (handle) {
            curl_easy_cleanup(handle);
            handle = nullptr;
        }
        if (headers) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }
    }
};

CurlRequest create_curl_request(const std::shared_ptr<spdlog::logger>& logger)
{
    CurlRequest request;
    request.handle = curl_easy_init();
    if (!request.handle) {
        if (logger) {
            logger->critical("Failed to initialize cURL handle for remote request");
        }
        throw std::runtime_error("Initialization Error: Failed to initialize cURL.");
    }

#ifdef _WIN32
    try {
        const auto cert_path = Utils::ensure_ca_bundle();
        curl_easy_setopt(request.handle, CURLOPT_CAINFO, cert_path.string().c_str());
    } catch (const std::exception& ex) {
        throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
    }
#endif
    return request;
}

void configure_request_payload(CurlRequest& request,
                               const std::string& api_url,
                               const std::string& payload,
                               const std::string& api_key,
                               long timeout_seconds,
                               std::string& response_buffer)
{
    curl_easy_setopt(request.handle, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(request.handle, CURLOPT_POST, 1L);
    curl_easy_setopt(request.handle, CURLOPT_TIMEOUT, timeout_seconds);

    request.headers = curl_slist_append(request.headers, "Content-Type: application/json");
    if (!api_key.empty()) {
        const std::string auth = "Authorization: Bearer " + api_key;
        request.headers = curl_slist_append(request.headers, auth.c_str());
    }
    curl_easy_setopt(request.handle, CURLOPT_HTTPHEADER, request.headers);

    curl_easy_setopt(request.handle, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(request.handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(request.handle, CURLOPT_WRITEDATA, &response_buffer);
}

long perform_request(CurlRequest& request, const std::shared_ptr<spdlog::logger>& logger)
{
    const CURLcode res = curl_easy_perform(request.handle);
    if (res != CURLE_OK) {
        if (logger) {
            logger->error("cURL request failed: {}", curl_easy_strerror(res));
        }
        throw std::runtime_error("Network Error: " + std::string(curl_easy_strerror(res)));
    }

    long http_code = 0;
    curl_easy_getinfo(request.handle, CURLINFO_RESPONSE_CODE, &http_code);
    return http_code;
}

std::string parse_category_response(const std::string& payload,
                                    long http_code,
                                    const std::shared_ptr<spdlog::logger>& logger)
{
    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::istringstream response_stream(payload);
    std::string errors;

    if (!Json::parseFromStream(reader_builder, response_stream, &root, &errors)) {
        if (logger) {
            logger->error("Failed to parse JSON response: {}", errors);
        }
        throw std::runtime_error("Response Error: Failed to parse JSON response. " + errors);
    }

    if (http_code == 401) {
        throw std::runtime_error("Authentication Error: Invalid or missing API key.");
    }
    if (http_code == 403) {
        throw std::runtime_error("Authorization Error: API key does not have sufficient permissions.");
    }
    if (http_code >= 500) {
        throw std::runtime_error("Server Error: Remote LLM server returned an error. Status code: " + std::to_string(http_code));
    }
    if (http_code >= 400) {
        const std::string error_message = root["error"]["message"].asString();
        throw std::runtime_error("Client Error: " + error_message);
    }

    return root["choices"][0]["message"]["content"].asString();
}
}


LLMClient::LLMClient(std::string api_key, std::string model, std::string base_url)
    : api_key(std::move(api_key)), model(std::move(model)), base_url(std::move(base_url))
{}


LLMClient::~LLMClient() = default;


void LLMClient::set_prompt_logging_enabled(bool enabled)
{
    prompt_logging_enabled = enabled;
}


std::string LLMClient::send_api_request(std::string json_payload) {
    std::string response_string;
    const std::string api_url = resolve_api_url();
    auto logger = Logger::get_logger("core_logger");

    if (logger) {
        logger->debug("Dispatching remote LLM request to {}", api_url);
    }

    CurlRequest request = create_curl_request(logger);
    configure_request_payload(request, api_url, json_payload, api_key, resolve_timeout_seconds(base_url), response_string);

    const long http_code = perform_request(request, logger);
    return parse_category_response(response_string, http_code, logger);
}

std::string LLMClient::effective_model() const
{
    return model.empty() ? "gpt-4o-mini" : model;
}

std::string LLMClient::resolve_api_url() const
{
    static const std::string kDefaultApi = "https://api.openai.com/v1/chat/completions";
    static const std::string kChatSuffix = "/chat/completions";

    if (base_url.empty()) {
        return kDefaultApi;
    }

    std::string trimmed = trim_ws(base_url);
    if (trimmed.empty()) {
        return kDefaultApi;
    }

    trimmed = trim_trailing_slashes(trimmed);
    if (ends_with(trimmed, kChatSuffix)) {
        return trimmed;
    }

    return trimmed + kChatSuffix;
}


std::string LLMClient::categorize_file(const std::string& file_name,
                                       const std::string& file_path,
                                       FileType file_type,
                                       const std::string& consistency_context)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting remote categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting remote categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }
    std::string json_payload = make_payload(file_name, file_path, file_type, consistency_context);

    if (prompt_logging_enabled && !last_prompt.empty()) {
        std::cout << "\n[DEV][PROMPT] Categorization request\n" << last_prompt << "\n";
    }

    std::string category = send_api_request(json_payload);

    if (prompt_logging_enabled) {
        std::cout << "[DEV][RESPONSE] Categorization reply\n" << category << "\n";
    }

    return category;
}


std::string LLMClient::make_payload(const std::string& file_name,
                                    const std::string& file_path,
                                    const FileType file_type,
                                    const std::string& consistency_context)
{
    std::string prompt;
    std::string sanitized_path = file_path;

    if (!sanitized_path.empty()) {
        prompt = "Categorize the item with full path: " + sanitized_path + "\n";
        prompt += "File name: " + file_name;
    } else {
        prompt = "Categorize file: " + file_name;
    }

    if (file_type == FileType::File) {
        // already set above
    } else {
        if (!sanitized_path.empty()) {
            prompt = "Categorize the directory with full path: " + sanitized_path + "\nDirectory name: " + file_name;
        } else {
            prompt = "Categorize directory: " + file_name;
        }
    }

    if (!consistency_context.empty()) {
        prompt += "\n\n" + consistency_context;
    }

    last_prompt = prompt;
    const std::string escaped_prompt = escape_json(prompt);
    const std::string system_prompt =
        "You are a file categorization assistant. If it's an installer, describe the type of software it installs. "
        "Consider the filename, extension, and any directory context provided. Always reply with one line in the "
        "format <Main category> : <Subcategory>. Main category must be broad (one or two words, plural). Subcategory "
        "must be specific, relevant, and must not repeat the main category.";
    const std::string escaped_system = escape_json(system_prompt);

    std::ostringstream payload;
    payload << "{\n"
            << "    \"model\": \"" << escape_json(effective_model()) << "\",\n"
            << "    \"messages\": [\n"
            << "        {\"role\": \"system\", \"content\": \"" << escaped_system << "\"},\n"
            << "        {\"role\": \"user\", \"content\": \"" << escaped_prompt << "\"}\n"
            << "    ]\n"
            << "}";

    return payload.str();
}

std::string LLMClient::make_generic_payload(const std::string& system_prompt,
                                            const std::string& user_prompt,
                                            int max_tokens) const
{
    std::ostringstream payload;
    payload << "{\"model\": \"" << escape_json(effective_model()) << "\",";
    payload << "\"messages\": [";
    payload << "{\"role\": \"system\", \"content\": \""
            << escape_json(system_prompt) << "\"},";
    payload << "{\"role\": \"user\", \"content\": \""
            << escape_json(user_prompt) << "\"}]";
    if (max_tokens > 0) {
        payload << ",\"max_tokens\": " << max_tokens;
    }
    payload << "}";
    return payload.str();
}

std::string LLMClient::complete_prompt(const std::string& prompt,
                                       int max_tokens)
{
    static const std::string kSystem =
        "You are a precise assistant that returns well-formed JSON responses.";
    std::string json_payload = make_generic_payload(kSystem, prompt, max_tokens);
    return send_api_request(json_payload);
}
