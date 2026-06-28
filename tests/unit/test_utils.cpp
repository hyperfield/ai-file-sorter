#include <catch2/catch_test_macros.hpp>
#include "Utils.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
#include <optional>
#include <filesystem>
#include <fstream>

namespace {

struct LlmStorageOverrideGuard {
    std::string previous = Utils::get_llm_storage_directory_override();

    ~LlmStorageOverrideGuard()
    {
        Utils::set_llm_storage_directory_override(previous);
    }
};

} // namespace

TEST_CASE("get_file_name_from_url extracts filename") {
    const std::string url = "https://example.com/models/mistral-7b.gguf";
    REQUIRE(Utils::get_file_name_from_url(url) == "mistral-7b.gguf");
}

TEST_CASE("get_file_name_from_url rejects malformed input") {
    REQUIRE_THROWS_AS(Utils::get_file_name_from_url("https://example.com/"), std::runtime_error);
}

TEST_CASE("LLM storage override changes default download destination") {
    LlmStorageOverrideGuard storage_guard;
    TempDir temp_dir;

    Utils::set_llm_storage_directory_override(temp_dir.path().string());

    const std::filesystem::path resolved =
        Utils::make_default_path_to_file_from_download_url("https://example.com/models/custom.gguf");
    CHECK(resolved == temp_dir.path() / "custom.gguf");
}

TEST_CASE("is_cuda_available honors probe overrides") {
    struct ProbeGuard {
        ~ProbeGuard() { TestHooks::reset_cuda_availability_probe(); }
    } guard;

    TestHooks::set_cuda_availability_probe([] { return true; });
    REQUIRE(Utils::is_cuda_available());

    TestHooks::set_cuda_availability_probe([] { return false; });
    REQUIRE_FALSE(Utils::is_cuda_available());
}

TEST_CASE("abbreviate_user_path strips home prefix") {
    TempDir temp_home;
    EnvVarGuard home_guard("HOME", temp_home.path().string());
    const auto file = temp_home.path() / "Documents" / "taxes.pdf";
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file).put('x');

    const std::string abbreviated =
        Utils::abbreviate_user_path(file.string());
    REQUIRE(abbreviated == "Documents/taxes.pdf");
}

TEST_CASE("sanitize_path_label strips invalid UTF-8 bytes") {
    std::string invalid = "Alpha";
    invalid.push_back(static_cast<char>(0xFF));
    invalid += "Beta";

    REQUIRE(Utils::sanitize_path_label(invalid) == "AlphaBeta");
}

TEST_CASE("sanitize_path_label preserves valid Unicode emoji labels") {
    const std::string label = "AB Testing ☁️";

    REQUIRE(Utils::sanitize_path_label(label) == label);
}

TEST_CASE("format_size keeps byte values in bytes below one kilobyte") {
    REQUIRE(Utils::format_size(999) == "999.00 B");
    REQUIRE(Utils::format_size(1024) == "1.00 KB");
}
