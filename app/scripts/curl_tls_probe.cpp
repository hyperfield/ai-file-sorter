#include "CurlTls.hpp"

#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr long kConnectTimeoutSeconds = 5;
constexpr long kRequestTimeoutSeconds = 15;

std::size_t discard_body(char*, std::size_t size, std::size_t count, void*)
{
    return size * count;
}
} // namespace

// Shared by the macOS build check and isolated TLS integration tests. Never installed.
int main(int argc, char** argv)
{
    try {
        CurlTls::initialize();
        struct GlobalCleanup {
            ~GlobalCleanup() { curl_global_cleanup(); }
        } global_guard;
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
        std::string url;
        std::string ca_file;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (i + 1 >= argc || (arg != "--url" && arg != "--ca-file")) {
                throw std::invalid_argument("Usage: curl-tls-probe [--url URL] [--ca-file FILE]");
            }
            (arg == "--url" ? url : ca_file) = argv[++i];
        }
        if (!handle) {
            throw std::runtime_error("Could not initialize cURL");
        }
        if (!ca_file.empty()) {
            curl_easy_setopt(handle.get(), CURLOPT_CAINFO, ca_file.c_str());
        }
        CurlTls::configure(handle.get());
        if (url.empty()) {
            std::puts(curl_version());
            return 0;
        }
        curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_PROXY, "");
        curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
        curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, kRequestTimeoutSeconds);
        curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, discard_body);
        char error[CURL_ERROR_SIZE] = {};
        curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, error);
        const auto result = curl_easy_perform(handle.get());
        long status = 0;
        curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &status);
        std::printf("curl_code=%d http_status=%ld\n", static_cast<int>(result), status);
        if (result != CURLE_OK) {
            std::fprintf(stderr, "%s\n", error[0] ? error : curl_easy_strerror(result));
        }
        return static_cast<int>(result);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
