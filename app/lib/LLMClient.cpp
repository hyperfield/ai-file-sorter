#include "LLMClient.hpp"
#include "Types.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#include <filesystem>
#ifdef _WIN32
    #include <json/json.h>
#elif __APPLE__
    #include <json/json.h>
#else
    #include <jsoncpp/json/json.h>
#endif

#include <iostream>
#include <sstream>

namespace {
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
}


// Helper function to write the response from curl into a string
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response)
{
    size_t totalSize = size * nmemb;
    response->append(static_cast<const char*>(contents), totalSize);
    return totalSize;
}


LLMClient::LLMClient(const std::string &api_key) : api_key(api_key)
{}


LLMClient::~LLMClient() = default;


std::string LLMClient::send_api_request(std::string json_payload) {
    CURL *curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api.openai.com/v1/chat/completions";
    auto logger = Logger::get_logger("core_logger");

    if (logger) {
        logger->debug("Dispatching remote LLM request to {}", api_url);
    }

    curl = curl_easy_init();
    if (!curl) {
        if (logger) {
            logger->critical("Failed to initialize cURL handle for remote request");
        }
        throw std::runtime_error("Initialization Error: Failed to initialize cURL.");
    }

    #ifdef _WIN32
        try {
            const auto cert_path = Utils::ensure_ca_bundle();
            curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
        } catch (const std::exception& ex) {
            curl_easy_cleanup(curl);
            throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
        }
    #endif
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        if (logger) {
            logger->error("cURL request failed: {}", curl_easy_strerror(res));
        }
        throw std::runtime_error("Network Error: " + std::string(curl_easy_strerror(res)));
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::istringstream response_stream(response_string);
    std::string errors;
    
    if (!Json::parseFromStream(reader_builder, response_stream, &root, &errors)) {
        if (logger) {
            logger->error("Failed to parse JSON response: {}", errors);
        }
        throw std::runtime_error("Response Error: Failed to parse JSON response. " + errors);
    }

    if (http_code == 401) {
        throw std::runtime_error("Authentication Error: Invalid or missing API key.");
    } else if (http_code == 403) {
        throw std::runtime_error("Authorization Error: API key does not have sufficient permissions.");
    } else if (http_code >= 500) {
        throw std::runtime_error("Server Error: OpenAI server returned an error. Status code: " + std::to_string(http_code));
    } else if (http_code >= 400) {
        std::string error_message = root["error"]["message"].asString();
        throw std::runtime_error("Client Error: " + error_message);
    }

    std::string category = root["choices"][0]["message"]["content"].asString();
    return category;
}


std::string LLMClient::categorize_file(const std::string& file_name,
                                       const std::string& file_path,
                                       FileType file_type)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting remote categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting remote categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }
    std::string json_payload = make_payload(file_name, file_path, file_type);

    return send_api_request(json_payload);
}


std::string LLMClient::make_payload(const std::string& file_name,
                                    const std::string& file_path,
                                    const FileType file_type)
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

    std::string escaped_prompt = escape_json(prompt);

    std::string json_payload = R"(
    {
        "model": "gpt-4o-mini",
        "messages": [
            {"role": "system", "content": "You are a file categorization assistant. If it's an installer, describe the type of software it installs. Consider the filename, extension, and any directory context provided. Always reply with one line in the format <Main category> : <Subcategory>. Main category must be broad (one or two words, plural). Subcategory must be specific, relevant, and must not repeat the main category."},
            {"role": "user", "content": ")" + escaped_prompt + R"("}
        ]
    }
    )";
    
    return json_payload;
}

std::string LLMClient::make_generic_payload(const std::string& system_prompt,
                                            const std::string& user_prompt,
                                            int max_tokens) const
{
    std::ostringstream payload;
    payload << "{\"model\": \"gpt-4o-mini\",";
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
