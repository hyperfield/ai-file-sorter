#include "LLMClient.h"

#include <curl/curl.h>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

// This file contains an improved LLMClient implementation with:
// - per-model token-bucket rate limiting and queuing
// - Retry-After header handling (seconds or HTTP-date)
// - adaptive timeouts via EWMA per model
// - persistent per-model state saved to disk
// - exponential backoff + jitter for retries

namespace llm {

static inline uint64_t now_ms() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

struct HttpResponse {
    long status = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    uint64_t duration_ms = 0;
};

// Helper to parse HTTP-date per RFC7231 into epoch seconds. Very small parser
// that handles common formats like: Tue, 15 Nov 1994 08:12:31 GMT
static time_t parse_http_date(const std::string &s) {
    struct tm tm = {};
    // Try to parse: "%a, %d %b %Y %H:%M:%S GMT"
    // strptime isn't portable across Windows; but most Linux/macOS support it.
#if defined(_MSC_VER)
    // minimal fallback: return 0 (can't parse) so caller will ignore
    (void)s;
    return 0;
#else
    if (strptime(s.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) != nullptr) {
        return timegm(&tm);
    }
    return 0;
#endif
}

class PersistentState {
public:
    struct ModelState {
        double tokens = 1.0;            // current tokens in bucket
        double capacity = 5.0;          // max tokens
        double refill_per_sec = 1.0;    // tokens added per second
        uint64_t last_refill_ms = 0;    // epoch ms
        uint64_t retry_after_until_ms = 0; // if > now_ms(), requests must wait
        double ewma_ms = 1000.0;        // EWMA of response duration
    };

    PersistentState(const std::string &path) : path_(path) { load(); }

    void load() {
        std::lock_guard<std::mutex> g(mu_);
        std::ifstream in(path_);
        if (!in) return; // no file yet
        states_.clear();
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            // Format: model\t tokens capacity refill last_refill retry_until ewma
            std::istringstream ss(line);
            std::string model;
            ModelState s;
            if (!(ss >> std::quoted(model) >> s.tokens >> s.capacity >> s.refill_per_sec >> s.last_refill_ms >> s.retry_after_until_ms >> s.ewma_ms)) continue;
            states_[model] = s;
        }
    }

    void save() {
        std::lock_guard<std::mutex> g(mu_);
        std::ofstream out(path_ + ".tmp");
        if (!out) return;
        for (auto &p : states_) {
            out << std::quoted(p.first) << ' ' << p.second.tokens << ' ' << p.second.capacity << ' ' << p.second.refill_per_sec << ' ' << p.second.last_refill_ms << ' ' << p.second.retry_after_until_ms << ' ' << p.second.ewma_ms << '\n';
        }
        out.close();
        std::rename((path_ + ".tmp").c_str(), path_.c_str());
    }

    ModelState get(const std::string &model) {
        std::lock_guard<std::mutex> g(mu_);
        if (states_.count(model)) return states_[model];
        ModelState s;
        s.tokens = s.capacity; // start full
        s.last_refill_ms = now_ms();
        states_[model] = s;
        return s;
    }

    void put(const std::string &model, const ModelState &s) {
        {
            std::lock_guard<std::mutex> g(mu_);
            states_[model] = s;
        }
        // Persist opportunistically but asynchronously to avoid blocking callers
        schedule_save();
    }

private:
    void schedule_save() {
        // Debounce saves: if a save worker already exists, let it handle persistence
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

// Simple curl wrapper for a single request with timeout header capture
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total = size * nitems;
    std::string h(buffer, total);
    auto *map = static_cast<std::map<std::string, std::string> *>(userdata);
    auto colon = h.find(':');
    if (colon != std::string::npos) {
        std::string key = h.substr(0, colon);
        // trim
        auto first = colon + 1;
        while (first < h.size() && std::isspace((unsigned char)h[first])) ++first;
        auto last = h.size();
        while (last > first && std::isspace((unsigned char)h[last - 1])) --last;
        std::string value = h.substr(first, last - first);
        // normalize header name
        for (auto &c : key) c = std::tolower(c);
        (*map)[key] = value;
    }
    return total;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    auto *s = static_cast<std::string *>(userdata);
    s->append(ptr, total);
    return total;
}

class LLMClientImpl {
public:
    LLMClientImpl(const std::string &persist_path = ".llm_state.txt") : state_(persist_path) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        std::random_device rd;
        rng_.seed(rd());
    }

    ~LLMClientImpl() { curl_global_cleanup(); }

    // A simple typed response with success flag
    struct Result {
        bool ok = false;
        HttpResponse response;
        std::string error;
    };

    // Send a request to the LLM for a particular model. This method will:
    // - block until token is available (token-bucket), or until retry-after expires
    // - use adaptive timeout for the model
    // - persist model state updates
    Result send_request(const std::string &model, const std::string &url, const std::string &payload, const std::vector<std::string> &headers = {}) {
        // Acquire per-model state
        auto s = state_.get(model);

        // refill tokens based on elapsed time
        refill_tokens(s);

        // If in Retry-After window, wait until it's passed
        uint64_t now = now_ms();
        if (s.retry_after_until_ms > now) {
            uint64_t wait_ms = s.retry_after_until_ms - now;
            // Wait but also respond to cancellation if necessary
            std::unique_lock<std::mutex> lk(cv_mu_);
            cv_.wait_for(lk, std::chrono::milliseconds(wait_ms));
            // reload time
            now = now_ms();
            refill_tokens(s);
        }

        // If no tokens, wait until enough tokens are available (or retry-after)
        if (s.tokens < 1.0) {
            // compute time until 1 token
            double needed = 1.0 - s.tokens;
            uint64_t wait_ms = (uint64_t)std::ceil((needed / s.refill_per_sec) * 1000.0);
            std::unique_lock<std::mutex> lk(cv_mu_);
            cv_.wait_for(lk, std::chrono::milliseconds(wait_ms));
            refill_tokens(s);
        }

        // consume one token
        if (s.tokens >= 1.0) s.tokens -= 1.0;
        else s.tokens = 0.0; // should not happen

        // Compute adaptive timeout. Use EWMA: start with s.ewma_ms; set timeout = clamp(ewma*2, 1000, 120000)
        uint64_t timeout_ms = (uint64_t)std::round(s.ewma_ms * 2.0);
        const uint64_t MIN_TIMEOUT_MS = 1500;
        const uint64_t MAX_TIMEOUT_MS = 2 * 60 * 1000; // 2 minutes
        timeout_ms = std::max(MIN_TIMEOUT_MS, std::min(MAX_TIMEOUT_MS, timeout_ms));

        // Perform request with retries on 429/5xx using exponential backoff + jitter
        const int MAX_RETRIES = 5;
        int attempt = 0;
        Result res;
        while (attempt <= MAX_RETRIES) {
            attempt++;
            auto http = perform_http(url, payload, headers, timeout_ms);
            res.response = http;
            if (http.status >= 200 && http.status < 300) {
                // success -> update EWMA and persist
                update_ewma_and_state(model, s, http.duration_ms);
                res.ok = true;
                state_.put(model, s);
                return res;
            }

            // Check for Retry-After header
            if (http.headers.count("retry-after")) {
                std::string v = http.headers["retry-after"];
                uint64_t until_ms = 0;
                // try integer seconds
                try {
                    size_t pos = 0;
                    long sec = std::stol(v, &pos);
                    if (pos > 0) {
                        until_ms = now_ms() + (uint64_t)sec * 1000;
                    } else {
                        // maybe HTTP-date
                        time_t t = parse_http_date(v);
                        if (t > 0) until_ms = (uint64_t)t * 1000;
                    }
                } catch (...) {
                    time_t t = parse_http_date(v);
                    if (t > 0) until_ms = (uint64_t)t * 1000;
                }
                if (until_ms > s.retry_after_until_ms) {
                    s.retry_after_until_ms = until_ms;
                }
            }

            // For 429 / 503 etc, backoff
            if (http.status == 429 || (http.status >= 500 && http.status < 600)) {
                // Set retry-after default if none: exponential backoff
                if (s.retry_after_until_ms <= now_ms()) {
                    // base backoff in ms
                    uint64_t base = 500ULL * (1ULL << (std::min(attempt - 1, 6)));
                    // jitter 0.5-1.5x
                    std::uniform_real_distribution<double> dist(0.5, 1.5);
                    double mult = dist(rng_);
                    uint64_t jittered = (uint64_t)std::min<uint64_t>(base * mult, 60ULL * 1000ULL);
                    s.retry_after_until_ms = now_ms() + jittered;
                }
                // Persist and wait until retry_after
                state_.put(model, s);
                uint64_t wait_ms = 0;
                if (s.retry_after_until_ms > now_ms()) wait_ms = s.retry_after_until_ms - now_ms();
                if (wait_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
                    // after waiting, try again
                    refill_tokens(s);
                    continue;
                }
                // otherwise just loop for next attempt
            }

            // Non-retryable error: return with error
            res.error = "HTTP status " + std::to_string(http.status);
            // update EWMA using full timeout to penalize or measured duration if available
            if (http.duration_ms > 0) update_ewma_and_state(model, s, http.duration_ms);
            else update_ewma_and_state(model, s, timeout_ms);
            state_.put(model, s);
            return res;
        }

        // exhausted retries
        res.error = "exhausted retries";
        return res;
    }

private:
    HttpResponse perform_http(const std::string &url, const std::string &payload, const std::vector<std::string> &headers, uint64_t timeout_ms) {
        HttpResponse r;
        CURL *c = curl_easy_init();
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

        struct curl_slist *chunk = nullptr;
        for (const auto &h : headers) chunk = curl_slist_append(chunk, h.c_str());
        if (chunk) curl_easy_setopt(c, CURLOPT_HTTPHEADER, chunk);

        auto start = std::chrono::steady_clock::now();
        CURLcode code = curl_easy_perform(c);
        auto stop = std::chrono::steady_clock::now();
        uint64_t elapsed = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
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

        // move headers
        r.headers = std::move(resp_headers);

        if (chunk) curl_slist_free_all(chunk);
        curl_easy_cleanup(c);
        return r;
    }

    void refill_tokens(PersistentState::ModelState &s) {
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

    void update_ewma_and_state(const std::string &model, PersistentState::ModelState &s, uint64_t observed_ms) {
        // alpha chosen for moderate smoothing
        double alpha = 0.2;
        s.ewma_ms = alpha * (double)observed_ms + (1.0 - alpha) * s.ewma_ms;
        // keep EWMA sane
        if (s.ewma_ms < 100.0) s.ewma_ms = 100.0;
        if (s.ewma_ms > 5 * 60 * 1000) s.ewma_ms = 5 * 60 * 1000;

        // small capacity adaptation: if ewma indicates slow, reduce capacity slightly
        if (s.ewma_ms > 30 * 1000) {
            s.capacity = std::max(1.0, s.capacity * 0.9);
            s.refill_per_sec = std::max(0.1, s.refill_per_sec * 0.9);
        } else {
            s.capacity = std::min(20.0, s.capacity * 1.01);
            s.refill_per_sec = std::min(10.0, s.refill_per_sec * 1.01);
        }

        // persist changes
        state_.put(model, s);
    }

    PersistentState state_;
    std::mutex cv_mu_;
    std::condition_variable cv_;
    std::mt19937 rng_;
};

// Simple facade to match presumed header LLMClient.h

LLMClient::LLMClient() : impl_(new LLMClientImpl()) {}
LLMClient::~LLMClient() { delete impl_; }

LLMClient::Response LLMClient::send(const std::string &model, const std::string &url, const std::string &payload, const std::vector<std::string> &headers) {
    auto r = impl_->send_request(model, url, payload, headers);
    LLMClient::Response out;
    out.ok = r.ok;
    out.status = r.response.status;
    out.body = r.response.body;
    out.error = r.error;
    out.duration_ms = r.response.duration_ms;
    out.headers = r.response.headers;
    return out;
}

} // namespace llm
