#include "LLMClient.hpp"

#include <Logger.hpp>
#include <curl/curl.h>
#include <json/json.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

namespace {

constexpr const char* OPENAI_API_URL = "https://api.openai.com/v1/chat/completions";
constexpr int kMaxRetries = 5;
constexpr uint64_t kMinTimeoutMs = 15000;  // 15 seconds minimum
constexpr uint64_t kMaxTimeoutMs = 180000; // 3 minutes maximum

inline uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Per-model state for adaptive timeout and rate limiting
struct ModelState {
    double tokens = 5.0;            // current tokens in bucket
    double capacity = 10.0;         // max tokens
    double refill_per_sec = 2.0;    // tokens added per second
    uint64_t last_refill_ms = 0;    // epoch ms
    uint64_t retry_after_until_ms = 0; // if > now_ms(), requests must wait
    double ewma_ms = 10000.0;       // EWMA of response duration
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
        if (save_pending_.exchange(true)) return;  // Already pending
        
        // Copy what we need by value to avoid use-after-free
        std::string path_copy = path_;
        std::map<std::string, ModelState> states_copy;
        {
            std::lock_guard<std::mutex> g(mu_);
            states_copy = states_;
        }
        
        // Capture atomic reference for safe reset from detached thread
        std::atomic<bool>* flag_ptr = &save_pending_;
        
        std::thread([path_copy = std::move(path_copy), 
                     states_copy = std::move(states_copy),
                     flag_ptr]() mutable {
            std::this_thread::sleep_for(250ms);
            
            // Perform save with copied data
            std::ofstream out(path_copy + ".tmp");
            if (out) {
                for (auto& p : states_copy) {
                    out << std::quoted(p.first) << ' ' << p.second.tokens << ' ' 
                        << p.second.capacity << ' ' << p.second.refill_per_sec << ' ' 
                        << p.second.last_refill_ms << ' ' << p.second.retry_after_until_ms << ' ' 
                        << p.second.ewma_ms << '\n';
                }
                out.close();
                std::rename((path_copy + ".tmp").c_str(), path_copy.c_str());
            }
            
            // Reset flag after save completes
            flag_ptr->store(false);
        }).detach();
    }

    std::string path_;
    std::map<std::string, ModelState> states_;
    std::mutex mu_;
    std::mutex save_mu_;
    std::atomic<bool> save_pending_{false};
};

static PersistentState& get_state() {
    static PersistentState state(".llm_state.txt");
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
    double alpha = 0.2;
    s.ewma_ms = alpha * (double)observed_ms + (1.0 - alpha) * s.ewma_ms;
    s.ewma_ms = std::max(100.0, std::min(300000.0, s.ewma_ms));
    
    // Adapt capacity based on performance
    if (s.ewma_ms > 30000) {
        s.capacity = std::max(1.0, s.capacity * 0.95);
        s.refill_per_sec = std::max(0.1, s.refill_per_sec * 0.95);
    } else {
        s.capacity = std::min(20.0, s.capacity * 1.02);
        s.refill_per_sec = std::min(10.0, s.refill_per_sec * 1.02);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(s.retry_after_until_ms - now));
        now = now_ms();
        refill_tokens(s);
    }
    
    // Wait for token availability
    if (s.tokens < 1.0) {
        double needed = 1.0 - s.tokens;
        uint64_t wait_ms = (uint64_t)std::ceil((needed / s.refill_per_sec) * 1000.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        refill_tokens(s);
    }
    
    // Consume token
    if (s.tokens >= 1.0) s.tokens -= 1.0;
    else s.tokens = 0.0;
    
    // Compute adaptive timeout
    uint64_t timeout_ms = (uint64_t)std::round(s.ewma_ms * 2.5);
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
        
        // Retry on 429 or 5xx
        if (http.status == 429 || (http.status >= 500 && http.status < 600)) {
            if (s.retry_after_until_ms <= now_ms()) {
                uint64_t base = 1000ULL * (1ULL << attempt);
                std::uniform_real_distribution<double> dist(0.5, 1.5);
                uint64_t jittered = (uint64_t)(base * dist(rng));
                s.retry_after_until_ms = now_ms() + std::min(jittered, 60000ULL);
            }
            state.put(model, s);
            
            uint64_t wait = 0;
            if (s.retry_after_until_ms > now_ms()) {
                wait = s.retry_after_until_ms - now_ms();
            }
            if (wait > 0) {
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

namespace {
// Ensure curl is initialized once per process
struct CurlGlobalInit {
    CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInit() { curl_global_cleanup(); }
};

void ensure_curl_initialized() {
    static CurlGlobalInit curl_init;
    (void)curl_init;  // Explicitly mark as used
}
} // anonymous namespace

LLMClient::LLMClient(std::string api_key, std::string model)
    : api_key(std::move(api_key)), model(std::move(model)) {
    ensure_curl_initialized();
}

LLMClient::~LLMClient() = default;

std::string LLMClient::effective_model() const {
    if (!model.empty()) return model;
    return "gpt-4o-mini";
}

std::string LLMClient::make_payload(const std::string& file_name,
                                   const std::string& file_path,
                                   const FileType file_type,
                                   const std::string& consistency_context) const {
    Json::Value root;
    root["model"] = effective_model();
    root["temperature"] = 0.0;
    root["max_tokens"] = 100;
    
    Json::Value messages(Json::arrayValue);
    
    Json::Value system_msg;
    system_msg["role"] = "system";
    std::string system_content = "You are an intelligent file categorization assistant. "
        "Analyze the file name, extension, and context to understand what the file represents. "
        "Consider the purpose, content type, and intended use of the file. "
        "Return ONLY a category and subcategory in the format: Category : Subcategory. "
        "No explanations, no additional text.";
    
    if (!consistency_context.empty()) {
        system_content += "\n\nContext and constraints:\n" + consistency_context;
    }
    system_msg["content"] = system_content;
    messages.append(system_msg);
    
    Json::Value user_msg;
    user_msg["role"] = "user";
    
    // Enhanced user message with file analysis
    std::string user_content = "File to categorize:\n";
    user_content += "Type: " + to_string(file_type) + "\n";
    user_content += "Name: " + file_name + "\n";
    if (!file_path.empty() && file_path != file_name) {
        user_content += "Path: " + file_path + "\n";
    }
    
    // Extract and analyze file extension
    size_t dot_pos = file_name.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < file_name.length() - 1) {
        std::string extension = file_name.substr(dot_pos + 1);
        user_content += "\nAnalyze this file based on:\n";
        user_content += "- What this file type (." + extension + ") is typically used for\n";
        user_content += "- The semantic meaning of the filename\n";
        user_content += "- Common purposes and applications for this file format\n";
    }
    
    user_msg["content"] = user_content;
    messages.append(user_msg);
    
    root["messages"] = messages;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string LLMClient::make_generic_payload(const std::string& system_prompt,
                                           const std::string& user_prompt,
                                           int max_tokens) const {
    Json::Value root;
    root["model"] = effective_model();
    root["temperature"] = 0.0;
    root["max_tokens"] = max_tokens;
    
    Json::Value messages(Json::arrayValue);
    
    if (!system_prompt.empty()) {
        Json::Value system_msg;
        system_msg["role"] = "system";
        system_msg["content"] = system_prompt;
        messages.append(system_msg);
    }
    
    Json::Value user_msg;
    user_msg["role"] = "user";
    user_msg["content"] = user_prompt;
    messages.append(user_msg);
    
    root["messages"] = messages;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string LLMClient::send_api_request(std::string json_payload) {
    std::vector<std::string> headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("Authorization: Bearer " + api_key);
    
    auto http = send_with_retry(effective_model(), OPENAI_API_URL, json_payload, headers);
    
    if (http.status < 200 || http.status >= 300) {
        throw std::runtime_error("OpenAI API request failed with status " + 
                               std::to_string(http.status) + ": " + http.body);
    }
    
    Json::CharReaderBuilder builder;
    Json::Value response;
    std::istringstream ss(http.body);
    std::string errors;
    
    if (!Json::parseFromStream(builder, ss, &response, &errors)) {
        throw std::runtime_error("Failed to parse API response: " + errors);
    }
    
    if (!response.isMember("choices") || response["choices"].empty()) {
        throw std::runtime_error("API response missing choices");
    }
    
    auto& choice = response["choices"][0];
    if (!choice.isMember("message") || !choice["message"].isMember("content")) {
        throw std::runtime_error("API response missing message content");
    }
    
    auto content = choice["message"]["content"].asString();
    
    if (prompt_logging_enabled) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("API Response: {}", content);
        }
    }
    
    return content;
}

std::string LLMClient::categorize_file(const std::string& file_name,
                                      const std::string& file_path,
                                      FileType file_type,
                                      const std::string& consistency_context) {
    auto payload = make_payload(file_name, file_path, file_type, consistency_context);
    
    if (prompt_logging_enabled) {
        last_prompt = payload;
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("Sending categorization request for: {}", file_name);
        }
    }
    
    return send_api_request(payload);
}

std::string LLMClient::complete_prompt(const std::string& prompt, int max_tokens) {
    auto payload = make_generic_payload("", prompt, max_tokens);
    
    if (prompt_logging_enabled) {
        last_prompt = payload;
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("Sending completion request");
        }
    }
    
    return send_api_request(payload);
}

void LLMClient::set_prompt_logging_enabled(bool enabled) {
    prompt_logging_enabled = enabled;
}
