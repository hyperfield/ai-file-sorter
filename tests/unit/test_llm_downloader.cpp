#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>

#include "LLMDownloader.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"

TEST_CASE("LLMDownloader retries full download after a range error") {
    TempDir tmp;
    const auto destination = (tmp.path() / "model.gguf").string();

    {
        std::ofstream out(destination, std::ios::binary);
        out << "abc";
    }

    LLMDownloader downloader("http://example.com/model.gguf");
    LLMDownloader::LLMDownloaderTestAccess::set_download_destination(downloader, destination);
    LLMDownloader::LLMDownloaderTestAccess::set_resume_headers(downloader, 6);

    std::atomic<int> attempts{0};
    TestHooks::set_llm_download_probe(
        [&](long offset, const std::string& path) -> CURLcode {
            ++attempts;
            if (attempts == 1) {
                REQUIRE(offset == 3);
                return CURLE_HTTP_RANGE_ERROR;
            }
            REQUIRE(offset == 0);
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << "abcdef";
            return CURLE_OK;
        });

    std::promise<void> done;
    std::future<void> finished = done.get_future();
    std::atomic<bool> success{false};
    std::string error_text;

    downloader.start_download(
        [](double) {},
        [&]() {
            success = true;
            done.set_value();
        },
        nullptr,
        [&](const std::string& err) {
            error_text = err;
            done.set_value();
        });

    const auto status = finished.wait_for(std::chrono::seconds(2));
    TestHooks::reset_llm_download_probe();
    REQUIRE(status == std::future_status::ready);
    REQUIRE(error_text.empty());
    REQUIRE(success.load());
    REQUIRE(attempts.load() == 2);
    REQUIRE(std::filesystem::file_size(destination) == 6);
}
