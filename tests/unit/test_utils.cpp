#include <catch2/catch_test_macros.hpp>
#include "Utils.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
#include <optional>
#include <filesystem>
#include <fstream>

TEST_CASE("get_file_name_from_url extracts filename") {
    const std::string url = "https://example.com/models/mistral-7b.gguf";
    REQUIRE(Utils::get_file_name_from_url(url) == "mistral-7b.gguf");
}

TEST_CASE("get_file_name_from_url rejects malformed input") {
    REQUIRE_THROWS_AS(Utils::get_file_name_from_url("https://example.com/"), std::runtime_error);
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
