#include "OllamaCloudProvider.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#ifdef _WIN32
    #include <json/json.h>
#elif __APPLE__
    #include <json/json.h>
#else
    #include <jsoncpp/json/json.h>
#endif
#include <sstream>
#include <cstdlib>

namespace {

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total = size * nmemb;
    response->append(static_cast<const char*>(contents), total);
    return total;
}

}

OllamaCloudProvider::OllamaCloudProvider(std::string base_url)
    : base_url_(std::move(base_url))
{
    logger_ = Logger::get_logger("core_logger");
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

ProviderKind OllamaCloudProvider::kind() const {
    return ProviderKind::OllamaCloud;
}

std::string OllamaCloudProvider::display_name() const {
    return "Ollama Cloud";
}

std::string OllamaCloudProvider::base_url() const {
    return base_url_;
}

bool OllamaCloudProvider::requires_api_key() const {
    return true;
}

bool OllamaCloudProvider::has_api_key() const {
    return !resolve_api_key().empty();
}

void OllamaCloudProvider::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}

void OllamaCloudProvider::set_api_key(const std::string& key) {
    api_key_ = key;
}

std::optional<std::string> OllamaCloudProvider::get_api_key_from_env() {
    const char* key = std::getenv("OLLAMA_API_KEY");
    if (key && key[0] != '\0') {
        return std::string(key);
    }
    return std::nullopt;
}

std::string OllamaCloudProvider::resolve_api_key() const {
    if (!api_key_.empty()) {
        return api_key_;
    }
    auto env_key = get_api_key_from_env();
    return env_key.value_or("");
}

std::string OllamaCloudProvider::perform_get(const std::string& endpoint, long& http_code) {
    std::string response;
    http_code = 0;

    if (base_url_.empty()) {
        if (logger_) {
            logger_->error("Ollama Cloud base URL not configured");
        }
        return "";
    }

    std::string api_key = resolve_api_key();
    if (api_key.empty()) {
        if (logger_) {
            logger_->error("Ollama Cloud API key not found (set OLLAMA_API_KEY environment variable)");
        }
        return "";
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (logger_) {
            logger_->error("Failed to initialize cURL for Ollama Cloud request");
        }
        return "";
    }

    std::string url = base_url_ + endpoint;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (logger_) {
        logger_->debug("Ollama Cloud GET {} (base: {})", endpoint, base_url_);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        if (logger_) {
            logger_->warn("Ollama Cloud request to {} failed: {}", url, curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        return "";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (logger_) {
        logger_->debug("Ollama Cloud GET {} returned HTTP {}", endpoint, http_code);
    }

    return response;
}

std::vector<OllamaModel> OllamaCloudProvider::parse_tags_response(const std::string& json_body) {
    std::vector<OllamaModel> models;

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::istringstream stream(json_body);
    std::string errors;

    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        if (logger_) {
            logger_->warn("Failed to parse Ollama Cloud tags response: {}", errors);
        }
        return models;
    }

    const Json::Value& models_arr = root["models"];
    if (!models_arr.isArray()) {
        if (logger_) {
            logger_->warn("Ollama Cloud tags response missing 'models' array");
        }
        return models;
    }

    for (const auto& item : models_arr) {
        OllamaModel m;
        m.name = item["name"].asString();
        m.digest = item.get("digest", "").asString();
        m.size_bytes = item.get("size", 0ULL).asUInt64();

        const Json::Value& details = item["details"];
        if (details.isObject()) {
            m.family = details.get("family", "").asString();
            m.parameter_size = details.get("parameter_size", "").asString();
            m.quantization_level = details.get("quantization_level", "").asString();
        }

        if (!m.name.empty()) {
            models.push_back(std::move(m));
        }
    }

    if (logger_) {
        logger_->info("Parsed {} models from Ollama Cloud", models.size());
    }

    return models;
}

HealthResult OllamaCloudProvider::check_health() {
    if (base_url_.empty()) {
        return HealthResult::error("Ollama Cloud base URL not configured", 0);
    }

    if (!has_api_key()) {
        return HealthResult::error("OLLAMA_API_KEY environment variable not set", 0);
    }

    long http_code = 0;
    std::string response = perform_get("/api/tags", http_code);

    if (http_code == 0) {
        return HealthResult::error("Connection failed - cannot reach " + base_url_, 0);
    }

    if (http_code == 401) {
        return HealthResult::error("Authentication failed - check your OLLAMA_API_KEY", 401);
    }

    if (http_code == 403) {
        return HealthResult::error("Access denied - API key may lack permissions", 403);
    }

    if (http_code >= 200 && http_code < 300) {
        return HealthResult::success();
    }

    return HealthResult::error("Ollama Cloud returned HTTP " + std::to_string(http_code), static_cast<int>(http_code));
}

ListModelsResult OllamaCloudProvider::list_models() {
    if (base_url_.empty()) {
        return ListModelsResult::error("Ollama Cloud base URL not configured");
    }

    if (!has_api_key()) {
        return ListModelsResult::error("OLLAMA_API_KEY environment variable not set");
    }

    long http_code = 0;
    std::string response = perform_get("/api/tags", http_code);

    if (http_code == 0) {
        return ListModelsResult::error("Connection failed - cannot reach " + base_url_);
    }

    if (http_code == 401) {
        return ListModelsResult::error("Authentication failed - check your OLLAMA_API_KEY");
    }

    if (http_code == 403) {
        return ListModelsResult::error("Access denied - API key may lack permissions");
    }

    if (http_code < 200 || http_code >= 300) {
        return ListModelsResult::error("Ollama Cloud returned HTTP " + std::to_string(http_code));
    }

    std::vector<OllamaModel> models = parse_tags_response(response);
    return ListModelsResult::success(std::move(models));
}
