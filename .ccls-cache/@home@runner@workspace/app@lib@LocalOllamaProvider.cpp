#include "LocalOllamaProvider.hpp"
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

namespace {

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total = size * nmemb;
    response->append(static_cast<const char*>(contents), total);
    return total;
}

}

LocalOllamaProvider::LocalOllamaProvider(std::string base_url)
    : base_url_(std::move(base_url))
{
    logger_ = Logger::get_logger("core_logger");
    if (base_url_.empty()) {
        base_url_ = "http://localhost:11434";
    }
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

ProviderKind LocalOllamaProvider::kind() const {
    return ProviderKind::LocalOllama;
}

std::string LocalOllamaProvider::display_name() const {
    return "Local Ollama";
}

std::string LocalOllamaProvider::base_url() const {
    return base_url_;
}

bool LocalOllamaProvider::requires_api_key() const {
    return false;
}

bool LocalOllamaProvider::has_api_key() const {
    return true;
}

void LocalOllamaProvider::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}

std::string LocalOllamaProvider::perform_get(const std::string& endpoint, long& http_code) {
    std::string response;
    http_code = 0;

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (logger_) {
            logger_->error("Failed to initialize cURL for local Ollama request");
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

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        if (logger_) {
            logger_->warn("Local Ollama request to {} failed: {}", url, curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        return "";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (logger_) {
        logger_->debug("Local Ollama GET {} returned HTTP {}", endpoint, http_code);
    }

    return response;
}

std::vector<OllamaModel> LocalOllamaProvider::parse_tags_response(const std::string& json_body) {
    std::vector<OllamaModel> models;

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::istringstream stream(json_body);
    std::string errors;

    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        if (logger_) {
            logger_->warn("Failed to parse Ollama tags response: {}", errors);
        }
        return models;
    }

    const Json::Value& models_arr = root["models"];
    if (!models_arr.isArray()) {
        if (logger_) {
            logger_->warn("Ollama tags response missing 'models' array");
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
        logger_->info("Parsed {} models from local Ollama", models.size());
    }

    return models;
}

HealthResult LocalOllamaProvider::check_health() {
    long http_code = 0;
    std::string response = perform_get("/api/tags", http_code);

    if (http_code == 0) {
        return HealthResult::error("Connection failed - is Ollama running at " + base_url_ + "?", 0);
    }

    if (http_code >= 200 && http_code < 300) {
        return HealthResult::success();
    }

    return HealthResult::error("Ollama returned HTTP " + std::to_string(http_code), static_cast<int>(http_code));
}

ListModelsResult LocalOllamaProvider::list_models() {
    long http_code = 0;
    std::string response = perform_get("/api/tags", http_code);

    if (http_code == 0) {
        return ListModelsResult::error("Connection failed - is Ollama running at " + base_url_ + "?");
    }

    if (http_code < 200 || http_code >= 300) {
        return ListModelsResult::error("Ollama returned HTTP " + std::to_string(http_code));
    }

    std::vector<OllamaModel> models = parse_tags_response(response);
    return ListModelsResult::success(std::move(models));
}
