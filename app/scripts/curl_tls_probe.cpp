#include "CurlTls.hpp"

#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr long kConnectTimeoutSeconds = 5;
constexpr long kRequestTimeoutSeconds = 15;

void require_option(CURLcode result)
{
    if (result != CURLE_OK) {
        throw std::runtime_error(curl_easy_strerror(result));
    }
}

std::size_t discard_body(char*, std::size_t size, std::size_t count, void*)
{
    return size * count;
}

std::map<std::string, std::string> parse_options(int argc, char** argv)
{
    std::map<std::string, std::string> options{
        {"--url", {}}, {"--proxy", {}}, {"--ca-file", {}}, {"--ca-dir", {}}, {"--ca-blob", {}},
        {"--test-ca-file", {}}, {"--head", {}}, {"--seed-revocation", {}}, {"--seed-insecure", {}}};
    for (int i = 1; i < argc; ++i) {
        const auto option = options.find(argv[i]);
        if (i + 1 >= argc || option == options.end()) {
            throw std::invalid_argument(
                "Usage: curl-tls-probe [--url URL] [--proxy URL] "
                "[--ca-file FILE] [--ca-dir DIR] [--ca-blob FILE]");
        }
        option->second = argv[++i];
    }
    return options;
}

void preset_anchors(CURL* handle, const std::map<std::string, std::string>& options)
{
    const auto& file = options.at("--ca-file");
    const auto& directory = options.at("--ca-dir");
    if (!file.empty()) {
        require_option(curl_easy_setopt(handle, CURLOPT_CAINFO, file.c_str()));
        require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, file.c_str()));
    }
    if (!directory.empty()) {
        // Secure Transport rejects directory anchors at assignment time.
        curl_easy_setopt(handle, CURLOPT_CAPATH, directory.c_str());
        curl_easy_setopt(handle, CURLOPT_PROXY_CAPATH, directory.c_str());
    }
#if LIBCURL_VERSION_NUM >= 0x074D00
    if (!options.at("--ca-blob").empty()) {
        std::ifstream input(options.at("--ca-blob"), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        curl_blob blob{const_cast<char*>(bytes.data()), bytes.size(), CURL_BLOB_COPY};
        if (!input.is_open() || bytes.empty()) {
            throw std::runtime_error("Cannot read CA blob fixture");
        }
        require_option(curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &blob));
        require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO_BLOB, &blob));
    }
#endif
}

void seed_revocation(CURL* handle, const std::string& mode)
{
    if (mode.empty()) {
        return;
    }
    if (mode != "no-revoke" && mode != "best-effort") {
        throw std::invalid_argument("Unknown revocation fixture mode");
    }
    const long flags = mode == "no-revoke" ? CURLSSLOPT_NO_REVOKE : CURLSSLOPT_REVOKE_BEST_EFFORT;
    require_option(curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, flags));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_SSL_OPTIONS, flags));
}

void install_test_anchors(CURL* handle, const std::string& file)
{
    if (file.empty()) {
        return;
    }
#ifdef _WIN32
    // This executable is a test-only target and is never installed. Schannel
    // creates exclusive memory stores from these per-handle fixture anchors.
    // Apply AFTER production policy only for signed-CRL/hostname/CONNECT tests;
    // separate tests prove production clears all preexisting anchors.
    require_option(curl_easy_setopt(handle, CURLOPT_CAINFO, file.c_str()));
    require_option(curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, file.c_str()));
#else
    throw std::invalid_argument("Test-only memory anchors require Schannel");
#endif
}
} // namespace

// Shared by build checks and isolated TLS integration tests. Never installed.
int main(int argc, char** argv)
{
    try {
        CurlTls::initialize();
        struct GlobalCleanup {
            ~GlobalCleanup() { curl_global_cleanup(); }
        } global_guard;
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
        const auto options = parse_options(argc, argv);
        const auto& url = options.at("--url");
        if (!handle) {
            throw std::runtime_error("Could not initialize cURL");
        }
        preset_anchors(handle.get(), options);
        seed_revocation(handle.get(), options.at("--seed-revocation"));
        if (!options.at("--seed-insecure").empty()) {
            require_option(curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 0L));
            require_option(curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 0L));
            require_option(curl_easy_setopt(handle.get(), CURLOPT_PROXY_SSL_VERIFYPEER, 0L));
            require_option(curl_easy_setopt(handle.get(), CURLOPT_PROXY_SSL_VERIFYHOST, 0L));
        }
        CurlTls::configure(handle.get());
        install_test_anchors(handle.get(), options.at("--test-ca-file"));
        std::puts(CurlTls::runtime_description().c_str());
        if (url.empty()) {
            return 0;
        }
        curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_PROXY, options.at("--proxy").c_str());
        curl_easy_setopt(handle.get(), CURLOPT_NOPROXY, "");
        curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_NOBODY, options.at("--head").empty() ? 0L : 1L);
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
