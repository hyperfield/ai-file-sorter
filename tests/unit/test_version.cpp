#include <catch2/catch_test_macros.hpp>

#include "Version.hpp"

#include <stdexcept>

TEST_CASE("Version formats optional prerelease tags")
{
    CHECK(Version{1, 8, 0}.to_string() == "1.8.0");
    CHECK(Version({1, 8, 0}, "beta").to_numeric_string() == "1.8.0");
    CHECK(Version({1, 8, 0}, "beta").to_string() == "1.8.0 beta");
    CHECK(Version({1, 8, 0}, "beta").has_prerelease());
}

TEST_CASE("Version parses prerelease labels from strings")
{
    CHECK(Version::parse("1.8.0 beta").to_string() == "1.8.0 beta");
    CHECK(Version::parse("1.8.0-beta").to_string() == "1.8.0 beta");
    CHECK(Version::parse("1.8.0b1").to_string() == "1.8.0 b1");
}

TEST_CASE("Version orders prerelease builds below stable releases")
{
    const Version beta({1, 8, 0}, "beta");
    const Version stable{1, 8, 0};

    CHECK(stable > beta);
    CHECK(stable >= beta);
    CHECK(beta <= stable);
    CHECK_FALSE(beta > stable);
    CHECK_FALSE(beta >= stable);
    CHECK(Version::parse("1.8.1") > beta);
}

TEST_CASE("Version rejects malformed numeric components")
{
    CHECK_THROWS_AS(Version::parse("beta"), std::runtime_error);
    CHECK_THROWS_AS(Version::parse("1..8"), std::runtime_error);
}
