#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>

#include "LLMDownloader.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"

namespace {

void write_bytes(const std::filesystem::path& path, std::size_t count) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    for (std::size_t i = 0; i < count; ++i) {
        out.put('x');
    }
}

void write_metadata(const std::filesystem::path& path,
                    const std::string& url,
                    long long content_length) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    out << "url=" << url << "\n";
    out << "content_length=" << content_length << "\n";
}

std::string make_unique_url() {
    return std::string("http://example.com/") + make_unique_token("model-") + ".gguf";
}

} // namespace

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

TEST_CASE("LLMDownloader uses cached metadata for partial downloads") {
    TempDir tmp;
    EnvVarGuard home_guard("HOME", tmp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", tmp.path().string());
#endif

    const std::string url = make_unique_url();
    const std::filesystem::path destination =
        Utils::make_default_path_to_file_from_download_url(url);
    const std::filesystem::path metadata = destination.string() + ".aifs.meta";

    write_bytes(destination, 4);
    write_metadata(metadata, url, 16);

    LLMDownloader downloader(url);
    CHECK(downloader.get_real_content_length() == 16);
    CHECK(downloader.get_local_download_status() == LLMDownloader::DownloadStatus::InProgress);
    CHECK(downloader.get_download_status() == LLMDownloader::DownloadStatus::InProgress);
    CHECK_FALSE(downloader.is_inited());
}

TEST_CASE("LLMDownloader resets to not started when local file is missing") {
    TempDir tmp;
    EnvVarGuard home_guard("HOME", tmp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", tmp.path().string());
#endif

    const std::string url = make_unique_url();
    const std::filesystem::path destination =
        Utils::make_default_path_to_file_from_download_url(url);
    const std::filesystem::path metadata = destination.string() + ".aifs.meta";

    write_metadata(metadata, url, 16);

    LLMDownloader downloader(url);
    CHECK(downloader.get_real_content_length() == 16);
    CHECK(downloader.get_local_download_status() == LLMDownloader::DownloadStatus::NotStarted);
    CHECK(downloader.get_download_status() == LLMDownloader::DownloadStatus::NotStarted);
    CHECK_FALSE(downloader.is_inited());
}

TEST_CASE("LLMDownloader treats full local file as complete with cached metadata") {
    TempDir tmp;
    EnvVarGuard home_guard("HOME", tmp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", tmp.path().string());
#endif

    const std::string url = make_unique_url();
    const std::filesystem::path destination =
        Utils::make_default_path_to_file_from_download_url(url);
    const std::filesystem::path metadata = destination.string() + ".aifs.meta";

    write_bytes(destination, 16);
    write_metadata(metadata, url, 16);

    LLMDownloader downloader(url);
    CHECK(downloader.get_real_content_length() == 16);
    CHECK(downloader.get_local_download_status() == LLMDownloader::DownloadStatus::Complete);
    CHECK(downloader.get_download_status() == LLMDownloader::DownloadStatus::Complete);
    CHECK_FALSE(downloader.is_inited());
}
