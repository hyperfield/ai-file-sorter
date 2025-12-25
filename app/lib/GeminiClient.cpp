#include "GeminiClient.hpp"

#include <Logger.hpp>
#include <curl/curl.h>
#include <json/json.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

namespace {

// Gemini API endpoint - using generateContent for free tier
constexpr const char* GEMINI_API_BASE = "https://generativelanguage.googleapis.com/v1beta/models/";
constexpr int kMaxRetries = 5;
// More conservative timeouts for free tier - Gemini free tier is slower
constexpr uint64_t kMinTimeoutMs = 20000;  // 20 seconds minimum
constexpr uint64_t kMaxTimeoutMs = 240000; // 4 minutes maximum for free tier
constexpr uint64_t kBaseBackoffMs = 2000;  // Base backoff time for retries
constexpr uint64_t kMaxBackoffMs = 120000; // Max backoff time (2 minutes)

inline uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Per-model state for adaptive timeout and rate limiting
// Optimized for Gemini free tier: 15 RPM (requests per minute)
struct ModelState {
    double tokens = 3.0;            // current tokens in bucket (lower for free tier)
    double capacity = 5.0;          // max tokens (conservative for free tier)
    double refill_per_sec = 0.25;   // ~15 per minute for free tier
    uint64_t last_refill_ms = 0;    // epoch ms
    uint64_t retry_after_until_ms = 0; // if > now_ms(), requests must wait
    double ewma_ms = 15000.0;       // EWMA of response duration (higher default for Gemini)
};

class PersistentState {
public:
    PersistentState(const std::string& path) : path_(path) { load(); }

    void load() {
        std::lock_guard<std::mutex> g(mu_);
        std::ifstream in(path_);
        if (!in) return;
        states_.clear();
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string model;
            ModelState s;
            if (!(ss >> std::quoted(model) >> s.tokens >> s.capacity >> s.refill_per_sec 
                  >> s.last_refill_ms >> s.retry_after_until_ms >> s.ewma_ms)) 
                continue;
            states_[model] = s;
        }
    }

    void save() {
        std::lock_guard<std::mutex> g(mu_);
        std::ofstream out(path_ + ".tmp");
        if (!out) return;
        for (auto& p : states_) {
            out << std::quoted(p.first) << ' ' << p.second.tokens << ' ' 
                << p.second.capacity << ' ' << p.second.refill_per_sec << ' ' 
                << p.second.last_refill_ms << ' ' << p.second.retry_after_until_ms << ' ' 
                << p.second.ewma_ms << '\n';
        }
        out.close();
        std::rename((path_ + ".tmp").c_str(), path_.c_str());
    }

    ModelState get(const std::string& model) {
        std::lock_guard<std::mutex> g(mu_);
        if (states_.count(model)) return states_[model];
        ModelState s;
        s.tokens = s.capacity;
        s.last_refill_ms = now_ms();
        states_[model] = s;
        return s;
    }

    void put(const std::string& model, const ModelState& s) {
        {
            std::lock_guard<std::mutex> g(mu_);
            states_[model] = s;
        }
        schedule_save();
    }

private:
    void schedule_save() {
        if (save_thread_.joinable()) return;
        save_thread_ = std::thread([this]() {
            std::this_thread::sleep_for(250ms);
            save();
        });
        save_thread_.detach();
    }

    std::string path_;
    std::map<std::string, ModelState> states_;
    std::mutex mu_;
    std::thread save_thread_;
};

static PersistentState& get_state() {
    static PersistentState state(".gemini_state.txt");
    return state;
}

void refill_tokens(ModelState& s) {
    uint64_t now = now_ms();
    if (s.last_refill_ms == 0) s.last_refill_ms = now;
    if (now <= s.last_refill_ms) return;
    double elapsed_s = (double)(now - s.last_refill_ms) / 1000.0;
    double add = elapsed_s * s.refill_per_sec;
    if (add > 0.0) {
        s.tokens = std::min(s.capacity, s.tokens + add);
        s.last_refill_ms = now;
    }
}

void update_ewma_and_state(const std::string& model, ModelState& s, uint64_t observed_ms) {
    double alpha = 0.15; // Slower adaptation for more stable free tier behavior
    s.ewma_ms = alpha * (double)observed_ms + (1.0 - alpha) * s.ewma_ms;
    s.ewma_ms = std::max(1000.0, std::min(300000.0, s.ewma_ms));
    
    // More conservative capacity adaptation for free tier
    if (s.ewma_ms > 40000) {
        s.capacity = std::max(1.0, s.capacity * 0.98);
        s.refill_per_sec = std::max(0.1, s.refill_per_sec * 0.98);
    } else if (s.ewma_ms < 15000) {
        s.capacity = std::min(10.0, s.capacity * 1.01);
        s.refill_per_sec = std::min(0.5, s.refill_per_sec * 1.01);
    }
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, total);
    return total;
}

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    std::string h(buffer, total);
    auto* map = static_cast<std::map<std::string, std::string>*>(userdata);
    auto colon = h.find(':');
    if (colon != std::string::npos) {
        std::string key = h.substr(0, colon);
        auto first = colon + 1;
        while (first < h.size() && std::isspace((unsigned char)h[first])) ++first;
        auto last = h.size();
        while (last > first && std::isspace((unsigned char)h[last - 1])) --last;
        std::string value = h.substr(first, last - first);
        for (auto& c : key) c = std::tolower(c);
        (*map)[key] = value;
    }
    return total;
}

struct HttpResponse {
    long status = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    uint64_t duration_ms = 0;
};

HttpResponse perform_http_request(const std::string& url, const std::string& payload,
                                  const std::vector<std::string>& headers, uint64_t timeout_ms) {
    HttpResponse r;
    CURL* c = curl_easy_init();
    if (!c) {
        r.status = 0;
        r.body = "curl init failed";
        return r;
    }
    
    std::string resp;
    std::map<std::string, std::string> resp_headers;
    
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)payload.size());
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &resp_headers);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    
    struct curl_slist* chunk = nullptr;
    for (const auto& h : headers) {
        chunk = curl_slist_append(chunk, h.c_str());
    }
    if (chunk) curl_easy_setopt(c, CURLOPT_HTTPHEADER, chunk);
    
    auto start = std::chrono::steady_clock::now();
    CURLcode code = curl_easy_perform(c);
    auto stop = std::chrono::steady_clock::now();
    uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    r.duration_ms = elapsed;
    
    if (code != CURLE_OK) {
        r.status = 0;
        r.body = curl_easy_strerror(code);
    } else {
        long http_code = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
        r.status = http_code;
        r.body = resp;
    }
    
    r.headers = std::move(resp_headers);
    
    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(c);
    return r;
}

HttpResponse send_with_retry(const std::string& model, const std::string& url, 
                             const std::string& payload, const std::vector<std::string>& headers) {
    auto& state = get_state();
    auto s = state.get(model);
    
    refill_tokens(s);
    
    // Wait for retry-after if needed
    uint64_t now = now_ms();
    if (s.retry_after_until_ms > now) {
        uint64_t wait_time = s.retry_after_until_ms - now;
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("Gemini rate limit: waiting {} seconds before next request", wait_time / 1000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        now = now_ms();
        refill_tokens(s);
    }
    
    // Wait for token availability
    if (s.tokens < 1.0) {
        double needed = 1.0 - s.tokens;
        uint64_t wait_ms = (uint64_t)std::ceil((needed / s.refill_per_sec) * 1000.0);
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("Gemini rate limiting: waiting {} seconds for token", wait_ms / 1000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        refill_tokens(s);
    }
    
    // Consume token
    if (s.tokens >= 1.0) s.tokens -= 1.0;
    else s.tokens = 0.0;
    
    // Compute adaptive timeout - more generous for free tier
    uint64_t timeout_ms = (uint64_t)std::round(s.ewma_ms * 3.0);
    timeout_ms = std::max(kMinTimeoutMs, std::min(kMaxTimeoutMs, timeout_ms));
    
    std::mt19937 rng(std::random_device{}());
    
    // Retry loop with exponential backoff
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        auto http = perform_http_request(url, payload, headers, timeout_ms);
        
        if (http.status >= 200 && http.status < 300) {
            update_ewma_and_state(model, s, http.duration_ms);
            state.put(model, s);
            return http;
        }
        
        // Handle Retry-After header
        if (http.headers.count("retry-after")) {
            try {
                long sec = std::stol(http.headers["retry-after"]);
                s.retry_after_until_ms = now_ms() + (uint64_t)sec * 1000;
            } catch (...) {}
        }
        
        // Retry on 429 (rate limit) or 5xx (server error)
        // Gemini free tier is more prone to rate limiting
        if (http.status == 429 || (http.status >= 500 && http.status < 600)) {
            if (s.retry_after_until_ms <= now_ms()) {
                // More aggressive backoff for free tier
                uint64_t base = kBaseBackoffMs * (1ULL << attempt);
                std::uniform_real_distribution<double> dist(0.7, 1.3);
                uint64_t jittered = (uint64_t)(base * dist(rng));
                s.retry_after_until_ms = now_ms() + std::min(jittered, kMaxBackoffMs);
            }
            state.put(model, s);
            
            uint64_t wait = 0;
            if (s.retry_after_until_ms > now_ms()) {
                wait = s.retry_after_until_ms - now_ms();
            }
            if (wait > 0) {
                if (auto logger = Logger::get_logger("core_logger")) {
                    logger->warn("Gemini API rate limit hit, waiting {} seconds (attempt {}/{})",
                               wait / 1000, attempt + 1, kMaxRetries);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                refill_tokens(s);
            }
            continue;
        }
        
        // Non-retryable error
        if (http.duration_ms > 0) {
            update_ewma_and_state(model, s, http.duration_ms);
        } else {
            update_ewma_and_state(model, s, timeout_ms);
        }
        state.put(model, s);
        return http;
    }
    
    // Exhausted retries
    HttpResponse fail;
    fail.status = 0;
    fail.body = "Exhausted retries";
    return fail;
}

} // anonymous namespace

GeminiClient::GeminiClient(std::string api_key, std::string model)
    : api_key(std::move(api_key)), model(std::move(model)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

GeminiClient::~GeminiClient() {
    curl_global_cleanup();
}

std::string GeminiClient::effective_model() const {
    if (!model.empty()) return model;
    return "gemini-1.5-flash";  // Default free tier model
}

std::string GeminiClient::make_payload(const std::string& file_name,
                                   const std::string& file_path,
                                   const FileType file_type,
                                   const std::string& consistency_context) {
    Json::Value root;
    
    // Gemini uses different structure than OpenAI
    Json::Value contents(Json::arrayValue);
    Json::Value part;
    Json::Value parts(Json::arrayValue);
    
    std::string prompt = "You are a file categorization assistant. "
        "Return ONLY a category and subcategory in the format: Category : Subcategory. "
        "No explanations, no additional text.\n\n";
    
    if (!consistency_context.empty()) {
        prompt += "For consistency, consider these recent categorizations:\n" + 
                 consistency_context + "\n\n";
    }
    
    prompt += "Categorize this " + to_string(file_type) + ": " + file_name;
    
    part["text"] = prompt;
    parts.append(part);
    
    Json::Value content;
    content["parts"] = parts;
    contents.append(content);
    
    root["contents"] = contents;
    
    // Generation config for concise output
    Json::Value generationConfig;
    generationConfig["temperature"] = 0.0;
    generationConfig["maxOutputTokens"] = 100;
    root["generationConfig"] = generationConfig;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string GeminiClient::make_generic_payload(const std::string& system_prompt,
                                           const std::string& user_prompt,
                                           int max_tokens) const {
    Json::Value root;
    
    Json::Value contents(Json::arrayValue);
    Json::Value parts(Json::arrayValue);
    Json::Value part;
    
    std::string full_prompt = system_prompt;
    if (!system_prompt.empty()) full_prompt += "\n\n";
    full_prompt += user_prompt;
    
    part["text"] = full_prompt;
    parts.append(part);
    
    Json::Value content;
    content["parts"] = parts;
    contents.append(content);
    
    root["contents"] = contents;
    
    Json::Value generationConfig;
    generationConfig["temperature"] = 0.0;
    generationConfig["maxOutputTokens"] = max_tokens;
    root["generationConfig"] = generationConfig;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string GeminiClient::send_api_request(std::string json_payload) {
    // Build Gemini API URL: models/{model}:generateContent?key={api_key}
    std::string url = std::string(GEMINI_API_BASE) + effective_model() + 
                     ":generateContent?key=" + api_key;
    
    std::vector<std::string> headers;
    headers.push_back("Content-Type: application/json");
    
    auto http = send_with_retry(effective_model(), url, json_payload, headers);
    
    if (http.status < 200 || http.status >= 300) {
        std::string error_msg = "Gemini API request failed with status " + 
                               std::to_string(http.status);
        if (!http.body.empty()) {
            error_msg += ": " + http.body;
        }
        throw std::runtime_error(error_msg);
    }
    
    Json::CharReaderBuilder builder;
    Json::Value response;
    std::istringstream ss(http.body);
    std::string errors;
    
    if (!Json::parseFromStream(builder, ss, &response, &errors)) {
        throw std::runtime_error("Failed to parse Gemini API response: " + errors);
    }
    
    // Gemini response structure: candidates[0].content.parts[0].text
    if (!response.isMember("candidates") || response["candidates"].empty()) {
        throw std::runtime_error("Gemini API response missing candidates");
    }
    
    auto& candidate = response["candidates"][0];
    if (!candidate.isMember("content") || !candidate["content"].isMember("parts") ||
        candidate["content"]["parts"].empty()) {
        throw std::runtime_error("Gemini API response missing content parts");
    }
    
    auto content = candidate["content"]["parts"][0]["text"].asString();
    
    if (prompt_logging_enabled) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("Gemini API Response: {}", content);
        }
    }
    
    return content;
}

std::string GeminiClient::categorize_file(const std::string& file_name,
                                      const std::string& file_path,
                                      FileType file_type,
                                      const std::string& consistency_context) {
    auto payload = make_payload(file_name, file_path, file_type, consistency_context);
    
    if (prompt_logging_enabled) {
        last_prompt = payload;
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("Sending Gemini categorization request for: {}", file_name);
        }
    }
    
    return send_api_request(payload);
}

std::string GeminiClient::complete_prompt(const std::string& prompt, int max_tokens) {
    auto payload = make_generic_payload("", prompt, max_tokens);
    
    if (prompt_logging_enabled) {
        last_prompt = payload;
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("Sending Gemini completion request");
        }
    }
    
    return send_api_request(payload);
}

void GeminiClient::set_prompt_logging_enabled(bool enabled) {
    prompt_logging_enabled = enabled;
}
