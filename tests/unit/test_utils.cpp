#include <catch2/catch_test_macros.hpp>
#include "Utils.hpp"

TEST_CASE("get_file_name_from_url extracts filename") {
    const std::string url = "https://example.com/models/mistral-7b.gguf";
    REQUIRE(Utils::get_file_name_from_url(url) == "mistral-7b.gguf");
}

TEST_CASE("get_file_name_from_url rejects malformed input") {
    REQUIRE_THROWS_AS(Utils::get_file_name_from_url("https://example.com/"), std::runtime_error);
}
