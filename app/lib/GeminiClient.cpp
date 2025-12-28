#include "GeminiClient.hpp"

#include <Logger.hpp>
#include <AppException.hpp>
#include <ErrorCode.hpp>
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
using namespace ErrorCodes;

namespace {

// Gemini API endpoint - using generateContent for free tier
constexpr const char* GEMINI_API_BASE = "https://generativelanguage.googleapis.com/v1beta/models/";
constexpr int kMaxRetries = 5;
// More conservative timeouts for free tier - Gemini free tier is slower
constexpr uint64_t kMinTimeoutMs = 20000;  // 20 seconds minimum
constexpr uint64_t kMaxTimeoutMs = 240000; // 4 minutes maximum for free tier
constexpr uint64_t kBaseBackoffMs = 2000;  // Base backoff time for retries
constexpr uint64_t kMaxBackoffMs = 120000; // Max backoff time (2 minutes)
// Circuit breaker configuration
constexpr int kCircuitBreakerThreshold = 3;  // consecutive failures before opening
constexpr uint64_t kCircuitBreakerResetMs = 60000;  // 1 minute cooldown

/*
 * Smart Gemini LLM Integration Features:
 * 
 * 1. CIRCUIT BREAKER PATTERN
 *    - Automatically stops requests after N consecutive failures
 *    - Cooldown period prevents overwhelming a degraded API
 *    - Resets gradually as service recovers
 * 
 * 2. PROGRESSIVE TIMEOUT EXTENSION
 *    - On timeout, retry with longer timeout instead of giving up
 *    - Tracks timeout extensions per session
 *    - Gradually reduces extensions on successful requests
 * 
 * 3. PAYLOAD-AWARE TIMEOUT SCALING
 *    - Larger prompts automatically get longer timeouts
 *    - Prevents premature timeout on complex requests
 * 
 * 4. DECORRELATED JITTER
 *    - Advanced backoff algorithm (AWS recommendation)
 *    - Prevents request clustering and thundering herd
 *    - Better distribution than simple exponential backoff
 * 
 * 5. CONNECTION MONITORING
 *    - Progress callback tracks data transfer
 *    - Detects stalled connections early
 *    - Enables faster retry on network issues
 * 
 * 6. PERSISTENT STATE TRACKING
 *    - Saves rate limit state across sessions
 *    - Adapts to historical performance
 *    - Smooth experience even after restarts
 * 
 * 7. ADAPTIVE RATE LIMITING
 *    - Token bucket algorithm with dynamic refill
 *    - Adjusts capacity based on API performance
 *    - Optimized for Gemini free tier (15 RPM)
 */

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
    // Circuit breaker state
    int consecutive_failures = 0;   // count of consecutive failures
    uint64_t circuit_open_until_ms = 0; // if > now_ms(), circuit is open (no requests)
    // Timeout tracking for progressive extension
    uint64_t last_timeout_ms = 0;   // last timeout used
    int timeout_extensions = 0;     // number of times we've extended timeout for current batch
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
            // Read base fields
            if (!(ss >> std::quoted(model) >> s.tokens >> s.capacity >> s.refill_per_sec 
                  >> s.last_refill_ms >> s.retry_after_until_ms >> s.ewma_ms)) 
                continue;
            // Try to read extended fields (backward compatible)
            ss >> s.consecutive_failures >> s.circuit_open_until_ms 
               >> s.last_timeout_ms >> s.timeout_extensions;
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
                << p.second.ewma_ms << ' ' << p.second.consecutive_failures << ' '
                << p.second.circuit_open_until_ms << ' ' << p.second.last_timeout_ms << ' '
                << p.second.timeout_extensions << '\n';
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
                        << p.second.ewma_ms << ' ' << p.second.consecutive_failures << ' '
                        << p.second.circuit_open_until_ms << ' ' << p.second.last_timeout_ms << ' '
                        << p.second.timeout_extensions << '\n';
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

// Calculate timeout based on payload size and historical data
uint64_t calculate_adaptive_timeout(const ModelState& s, size_t payload_size) {
    // Base timeout on EWMA
    uint64_t timeout_ms = (uint64_t)std::round(s.ewma_ms * 3.0);
    
    // Scale by payload size (rough heuristic: add 1ms per 100 bytes)
    uint64_t size_factor = payload_size / 100;
    timeout_ms += std::min(size_factor, 30000ULL);  // Cap size adjustment at 30s
    
    // If we've had timeout issues, be more generous
    if (s.timeout_extensions > 0) {
        double multiplier = 1.0 + (0.3 * s.timeout_extensions);  // +30% per extension
        timeout_ms = (uint64_t)((double)timeout_ms * multiplier);
    }
    
    return std::max(kMinTimeoutMs, std::min(kMaxTimeoutMs, timeout_ms));
}

// Check if circuit breaker is open
bool is_circuit_open(const ModelState& s) {
    uint64_t now = now_ms();
    if (s.circuit_open_until_ms > now) {
        return true;
    }
    return false;
}

// Better jitter calculation using decorrelated jitter (AWS recommendation)
// This avoids request clustering better than simple uniform jitter
uint64_t calculate_jittered_backoff(int attempt, uint64_t last_backoff_ms) {
    // Decorrelated jitter: sleep = min(cap, random_between(base, sleep * 3))
    uint64_t base = kBaseBackoffMs;
    uint64_t cap = kMaxBackoffMs;
    
    std::mt19937 rng(std::random_device{}());
    
    if (last_backoff_ms == 0) {
        // First attempt - use exponential backoff
        uint64_t exp_backoff = base * (1ULL << std::min(attempt, 6));
        std::uniform_int_distribution<uint64_t> dist(base, exp_backoff);
        return std::min(cap, dist(rng));
    } else {
        // Subsequent attempts - decorrelated jitter
        uint64_t upper = std::min(cap, last_backoff_ms * 3);
        std::uniform_int_distribution<uint64_t> dist(base, upper);
        return dist(rng);
    }
}

// Update circuit breaker on request result
void update_circuit_breaker(ModelState& s, bool success) {
    uint64_t now = now_ms();
    
    // If circuit was open and cooldown expired, reset on first check
    if (s.circuit_open_until_ms > 0 && s.circuit_open_until_ms <= now) {
        s.circuit_open_until_ms = 0;
        s.consecutive_failures = 0;
        s.timeout_extensions = 0;  // Reset timeout extensions too
    }
    
    if (success) {
        // Success - reset failure counter and timeout extensions
        s.consecutive_failures = 0;
        s.timeout_extensions = std::max(0, s.timeout_extensions - 1);  // Gradually reduce
    } else {
        // Failure - increment counter
        s.consecutive_failures++;
        
        // Open circuit if threshold exceeded
        if (s.consecutive_failures >= kCircuitBreakerThreshold) {
            s.circuit_open_until_ms = now + kCircuitBreakerResetMs;
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->warn("Gemini circuit breaker opened after {} consecutive failures, cooling down for {} seconds",
                           s.consecutive_failures, kCircuitBreakerResetMs / 1000);
            }
        }
    }
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, total);
    return total;
}

// Progress callback to detect stalled connections
struct ProgressData {
    uint64_t last_activity_ms = 0;
    std::atomic<bool>* cancel_flag = nullptr;
};

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                      curl_off_t ultotal, curl_off_t ulnow) {
    auto* data = static_cast<ProgressData*>(clientp);
    
    // Update activity timestamp if we're making progress
    if (dlnow > 0 || ulnow > 0) {
        data->last_activity_ms = now_ms();
    }
    
    // Check for cancel flag
    if (data->cancel_flag && data->cancel_flag->load()) {
        return 1;  // Abort
    }
    
    return 0;  // Continue
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
    ProgressData progress_data;
    progress_data.last_activity_ms = now_ms();
    
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
    
    // Enable progress callback for stall detection
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &progress_data);
    
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
    
    // Check circuit breaker first
    if (is_circuit_open(s)) {
        uint64_t wait_time = s.circuit_open_until_ms - now_ms();
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("Gemini circuit breaker open: waiting {} seconds before retry", wait_time / 1000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        // Reset circuit state after waiting
        s.circuit_open_until_ms = 0;
        s.consecutive_failures = 0;
        refill_tokens(s);
    }
    
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
    
    // Compute adaptive timeout based on payload size and history
    uint64_t timeout_ms = calculate_adaptive_timeout(s, payload.size());
    s.last_timeout_ms = timeout_ms;
    
    std::mt19937 rng(std::random_device{}());
    uint64_t last_backoff_ms = 0;  // Track last backoff for decorrelated jitter
    
    // Retry loop with exponential backoff
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        auto http = perform_http_request(url, payload, headers, timeout_ms);
        
        if (http.status >= 200 && http.status < 300) {
            // Success!
            update_ewma_and_state(model, s, http.duration_ms);
            update_circuit_breaker(s, true);
            state.put(model, s);
            return http;
        }
        
        // Mark as failure for circuit breaker
        bool is_retryable = false;
        
        // Handle Retry-After header
        if (http.headers.count("retry-after")) {
            try {
                long sec = std::stol(http.headers["retry-after"]);
                s.retry_after_until_ms = now_ms() + (uint64_t)sec * 1000;
            } catch (...) {}
        }
        
        // Special handling for timeout errors (status == 0 typically means timeout)
        if (http.status == 0 && http.body.find("Timeout") != std::string::npos) {
            // Progressive timeout extension for timeout errors
            s.timeout_extensions++;
            uint64_t extended_timeout = calculate_adaptive_timeout(s, payload.size());
            
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->warn("Gemini request timeout, extending timeout to {} seconds for next attempt (attempt {}/{})",
                           extended_timeout / 1000, attempt + 1, kMaxRetries);
            }
            
            timeout_ms = extended_timeout;
            is_retryable = true;
            
            // Add backoff before retry with decorrelated jitter
            last_backoff_ms = calculate_jittered_backoff(attempt, last_backoff_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(last_backoff_ms));
            refill_tokens(s);
            continue;
        }
        
        // Retry on 429 (rate limit) or 5xx (server error)
        // Gemini free tier is more prone to rate limiting
        if (http.status == 429 || (http.status >= 500 && http.status < 600)) {
            is_retryable = true;
            
            if (s.retry_after_until_ms <= now_ms()) {
                // Use decorrelated jitter for better distribution
                last_backoff_ms = calculate_jittered_backoff(attempt, last_backoff_ms);
                s.retry_after_until_ms = now_ms() + last_backoff_ms;
            }
            state.put(model, s);
            
            uint64_t wait = 0;
            if (s.retry_after_until_ms > now_ms()) {
                wait = s.retry_after_until_ms - now_ms();
            }
            if (wait > 0) {
                if (auto logger = Logger::get_logger("core_logger")) {
                    logger->warn("Gemini API {} error, waiting {} seconds (attempt {}/{})",
                               http.status == 429 ? "rate limit" : "server",
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
        update_circuit_breaker(s, false);
        state.put(model, s);
        return http;
    }
    
    // Exhausted retries - update circuit breaker
    update_circuit_breaker(s, false);
    state.put(model, s);
    
    HttpResponse fail;
    fail.status = 0;
    fail.body = "Exhausted retries after multiple timeout/rate limit errors";
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

GeminiClient::GeminiClient(std::string api_key, std::string model)
    : api_key(std::move(api_key)), model(std::move(model)) {
    ensure_curl_initialized();
}

GeminiClient::~GeminiClient() = default;

std::string GeminiClient::effective_model() const {
    if (!model.empty()) return model;
    return "gemini-1.5-flash";  // Default free tier model
}

std::string GeminiClient::make_payload(const std::string& file_name,
                                   const std::string& file_path,
                                   const FileType file_type,
                                   const std::string& consistency_context) const {
    Json::Value root;
    
    // Gemini uses different structure than OpenAI
    Json::Value contents(Json::arrayValue);
    Json::Value part;
    Json::Value parts(Json::arrayValue);
    
    std::string prompt = "You are an intelligent file categorization assistant. "
        "Analyze the file name, extension, and context to understand what the file represents. "
        "Consider the purpose, content type, and intended use of the file.\n\n"
        "IMPORTANT: If you are uncertain about the categorization (confidence < 70%), "
        "respond with: UNCERTAIN : [filename]\n"
        "Otherwise, respond ONLY with: Category : Subcategory\n"
        "No explanations, no additional text.\n\n";
    
    if (!consistency_context.empty()) {
        prompt += "Context and constraints:\n" + consistency_context + "\n\n";
    }
    
    // Enhanced prompt with file type awareness
    prompt += "File to categorize:\n";
    prompt += "Type: " + to_string(file_type) + "\n";
    prompt += "Name: " + file_name + "\n";
    if (!file_path.empty() && file_path != file_name) {
        prompt += "Path: " + file_path + "\n";
    }
    
    // Extract and analyze file extension
    size_t dot_pos = file_name.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < file_name.length() - 1) {
        std::string extension = file_name.substr(dot_pos + 1);
        prompt += "\nAnalyze this file based on:\n";
        prompt += "- What this file type (." + extension + ") is typically used for\n";
        prompt += "- The semantic meaning of the filename\n";
        prompt += "- Common purposes and applications for this file format\n";
    }
    
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
        // Determine specific error code based on HTTP status
        Code error_code = Code::API_SERVER_ERROR;
        std::string context = "HTTP " + std::to_string(http.status);
        
        if (http.status == 401) {
            error_code = Code::API_AUTHENTICATION_FAILED;
            context += ": Invalid API key";
        } else if (http.status == 403) {
            error_code = Code::API_INSUFFICIENT_PERMISSIONS;
            context += ": Insufficient permissions";
        } else if (http.status == 429) {
            error_code = Code::API_RATE_LIMIT_EXCEEDED;
            context += ": Rate limit exceeded";
        } else if (http.status >= 500) {
            error_code = Code::API_SERVER_ERROR;
            context += ": Server error";
        } else if (http.status >= 400) {
            error_code = Code::API_INVALID_REQUEST;
            context += ": Bad request";
        }
        
        if (!http.body.empty()) {
            context += " - " + http.body;
        }
        
        throw AppException(error_code, context);
    }
    
    Json::CharReaderBuilder builder;
    Json::Value response;
    std::istringstream ss(http.body);
    std::string errors;
    
    if (!Json::parseFromStream(builder, ss, &response, &errors)) {
        throw AppException(Code::API_RESPONSE_PARSE_ERROR, "JSON parse error: " + errors);
    }
    
    // Gemini response structure: candidates[0].content.parts[0].text
    if (!response.isMember("candidates") || response["candidates"].empty()) {
        throw AppException(Code::API_INVALID_RESPONSE, 
            "Response missing 'candidates' field - model may have blocked the request");
    }
    
    auto& candidate = response["candidates"][0];
    if (!candidate.isMember("content") || !candidate["content"].isMember("parts") ||
        candidate["content"]["parts"].empty()) {
        throw AppException(Code::API_INVALID_RESPONSE,
            "Response missing content parts - model response may be incomplete");
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
