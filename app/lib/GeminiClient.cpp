#include "GeminiClient.hpp"

#include "FolderTreeCatalog.hpp"

#include "Logger.hpp"
#include "RemoteApiError.hpp"
#include "Utils.hpp"

#include <curl/curl.h>

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
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response)
{
    const size_t total_size = size * nmemb;
    response->append(static_cast<const char*>(contents), total_size);
    return total_size;
}

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

struct HttpResponseInfo {
    long status_code{0};
    std::string retry_after;
};

std::string trim_ws(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

size_t HeaderCallback(char* buffer, size_t size, size_t nitems, std::string* retry_after)
{
    const size_t total_size = size * nitems;
    std::string line(buffer, total_size);
    const std::string prefix = "retry-after:";
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lower.rfind(prefix, 0) == 0) {
        *retry_after = trim_ws(line.substr(prefix.size()));
    }
    return total_size;
}

CurlRequest create_curl_request(const std::shared_ptr<spdlog::logger>& logger)
{
    CurlRequest request;
    request.handle = curl_easy_init();
    if (!request.handle) {
        if (logger) {
            logger->critical("Failed to initialize cURL handle for Gemini request");
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
                               std::string& response_buffer,
                               std::string& retry_after_header)
{
    curl_easy_setopt(request.handle, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(request.handle, CURLOPT_POST, 1L);
    curl_easy_setopt(request.handle, CURLOPT_TIMEOUT, 5L);

    request.headers = curl_slist_append(request.headers, "Content-Type: application/json");
    curl_easy_setopt(request.handle, CURLOPT_HTTPHEADER, request.headers);

    curl_easy_setopt(request.handle, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(request.handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(request.handle, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(request.handle, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(request.handle, CURLOPT_HEADERDATA, &retry_after_header);
}

HttpResponseInfo perform_request(CurlRequest& request,
                                 std::string retry_after_header,
                                 const std::shared_ptr<spdlog::logger>& logger)
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
    return HttpResponseInfo{http_code, std::move(retry_after_header)};
}

std::string parse_text_response(const std::string& payload,
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

    const auto& candidates = root["candidates"];
    if (!candidates.isArray() || candidates.empty()) {
        throw std::runtime_error("Response Error: Gemini response contained no candidates.");
    }
    const auto& parts = candidates[0]["content"]["parts"];
    if (!parts.isArray() || parts.empty()) {
        throw std::runtime_error("Response Error: Gemini response missing content parts.");
    }

    return parts[0]["text"].asString();
}

} // namespace


GeminiClient::GeminiClient(std::string api_key, std::string model)
    : api_key_(std::move(api_key)),
      model_(std::move(model))
{
}

void GeminiClient::set_prompt_logging_enabled(bool enabled)
{
    prompt_logging_enabled_ = enabled;
}

std::string GeminiClient::send_api_request(const std::string& json_payload)
{
    if (api_key_.empty()) {
        throw std::runtime_error("Missing Gemini API key.");
    }

    const std::string model_path = effective_model().starts_with("models/") ? effective_model()
                                                                            : "models/" + effective_model();
    const std::vector<std::string> api_versions = {"v1", "v1beta"};
    std::string last_error;

    for (size_t i = 0; i < api_versions.size(); ++i) {
        std::string response_string;
        const std::string base_url = "https://generativelanguage.googleapis.com/"
            + api_versions[i]
            + "/"
            + model_path
            + ":generateContent";
        const std::string api_url = base_url + "?key=" + api_key_;
        auto logger = Logger::get_logger("core_logger");

        if (logger) {
            logger->debug("Dispatching remote LLM request to {}", base_url);
        }

        try {
            CurlRequest request = create_curl_request(logger);
            std::string retry_after_header;
            configure_request_payload(request, api_url, json_payload, response_string, retry_after_header);
            const HttpResponseInfo response = perform_request(request, std::move(retry_after_header), logger);
            if (response.status_code == 404 && i + 1 < api_versions.size()) {
                // Fallback to next version (e.g., v1beta) on 404.
                last_error = "HTTP 404 on " + api_versions[i];
                continue;
            }
            if (response.status_code >= 400) {
                RemoteApiError::throw_for_http_error("Gemini",
                                                     response.status_code,
                                                     response_string,
                                                     response.retry_after,
                                                     logger);
            }
            return parse_text_response(response_string, logger);
        } catch (const std::exception& ex) {
            last_error = ex.what();
            if (i + 1 < api_versions.size()) {
                continue;
            }
            throw;
        }
    }

    throw std::runtime_error(last_error.empty() ? "Gemini request failed" : last_error);
}

std::string GeminiClient::effective_model() const
{
    return model_.empty() ? "gemini-2.5-flash-lite" : model_;
}

std::string GeminiClient::categorize_file(const std::string& file_name,
                                          const std::string& file_path,
                                          FileType file_type,
                                          const std::string& consistency_context)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting Gemini categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting Gemini categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }

    std::string json_payload = make_categorization_payload(file_name, file_path, file_type, consistency_context);

    if (prompt_logging_enabled_ && !last_prompt_.empty()) {
        std::cout << "\n[DEV][PROMPT] Categorization request\n" << last_prompt_ << "\n";
    }

    std::string category = send_api_request(json_payload);

    if (prompt_logging_enabled_) {
        std::cout << "[DEV][RESPONSE] Categorization reply\n" << category << "\n";
    }

    return category;
}

std::string GeminiClient::make_categorization_payload(const std::string& file_name,
                                                      const std::string& file_path,
                                                      FileType file_type,
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

    if (file_type == FileType::Directory) {
        if (!sanitized_path.empty()) {
            prompt = "Categorize the directory with full path: " + sanitized_path + "\nDirectory name: " + file_name;
        } else {
            prompt = "Categorize directory: " + file_name;
        }
    }

    if (!consistency_context.empty()) {
        prompt += "\n\n" + consistency_context;
    }

    last_prompt_ = prompt;
    const bool folder_tree_mode =
        consistency_context.find(FolderTreeCatalog::kPromptMarker) != std::string::npos;
    const std::string system_prompt = folder_tree_mode
        ? "You are a file organization assistant. The user prompt contains an existing folder tree. "
          "Choose the best destination folder under that tree. Always reply with only one JSON object "
          "in the format {\"targetFolder\":\"relative/folder/path\",\"createFolder\":false}. "
          "Do not return category/subcategory text and do not explain your answer."
        : "You are a file categorization assistant. If it's an installer, describe the type of software it installs. "
          "Consider the filename, extension, and any directory context provided. If the user prompt includes an "
          "'Allowed main categories' list, choose the main category from that list only. Use Other only when it is "
          "listed and none of the other listed main categories clearly fits. Always reply with one line in the "
          "format <Main category> : <Subcategory>. Main category must be broad (one or two words, plural). "
          "Subcategory must be specific, relevant, and must not repeat the main category.";

    std::ostringstream payload;
    const std::string merged_prompt = system_prompt + "\n\n" + prompt;

    payload << "{";
    payload << "\"contents\":[";
    payload << "{\"role\":\"user\",\"parts\":[{\"text\":\"" << escape_json(merged_prompt) << "\"}]}";
    payload << "]";
    payload << "}";

    return payload.str();
}

std::string GeminiClient::make_generic_payload(const std::string& system_prompt,
                                               const std::string& user_prompt,
                                               int max_tokens) const
{
    const std::string merged_prompt = system_prompt + "\n\n" + user_prompt;

    std::ostringstream payload;
    payload << "{";
    payload << "\"contents\":[";
    payload << "{\"role\":\"user\",\"parts\":[{\"text\":\"" << escape_json(merged_prompt) << "\"}]}";
    payload << "]";
    if (max_tokens > 0) {
        payload << ",\"generationConfig\":{\"maxOutputTokens\":" << max_tokens << "}";
    }
    payload << "}";
    return payload.str();
}

std::string GeminiClient::complete_prompt(const std::string& prompt,
                                          int max_tokens)
{
    static const std::string kSystem =
        "You are a precise assistant that returns well-formed JSON responses.";
    if (prompt_logging_enabled_) {
        std::cout << "\n[DEV][PROMPT] Completion request\n"
                  << prompt << "\n";
    }
    std::string json_payload = make_generic_payload(kSystem, prompt, max_tokens);
    std::string response = send_api_request(json_payload);
    if (prompt_logging_enabled_) {
        std::cout << "[DEV][RESPONSE] Completion reply\n" << response << "\n";
    }
    return response;
}
