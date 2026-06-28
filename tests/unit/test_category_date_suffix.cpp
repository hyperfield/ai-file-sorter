#include <catch2/catch_test_macros.hpp>

#include "CategoryDateSuffix.hpp"

TEST_CASE("CategoryDateSuffix appends generated date suffixes once")
{
    CHECK(CategoryDateSuffix::append_date_suffix("Documents", "2026-06") == "Documents_2026-06");
    CHECK(CategoryDateSuffix::append_date_suffix("Images", "2026-06-28") == "Images_2026-06-28");
    CHECK(CategoryDateSuffix::append_date_suffix("Documents_2026-06", "2026-06") ==
          "Documents_2026-06");
    CHECK(CategoryDateSuffix::append_date_suffix("", "2026-06").empty());
    CHECK(CategoryDateSuffix::append_date_suffix("Documents", "") == "Documents");
}

TEST_CASE("CategoryDateSuffix strips exact generated date suffixes")
{
    CHECK(CategoryDateSuffix::strip_date_suffix("Documents_2026-06", "2026-06") == "Documents");
    CHECK(CategoryDateSuffix::strip_date_suffix("Images_2026-06-28", "2026-06-28") == "Images");
    CHECK(CategoryDateSuffix::strip_date_suffix("Images_2026-06-28", "2026-06-27") ==
          "Images_2026-06-28");
}

TEST_CASE("CategoryDateSuffix strips generated suffixes by kind")
{
    const auto document = CategoryDateSuffix::strip_generated_suffix(
        "Documents_2026-06", CategoryDateSuffix::Kind::Document);
    REQUIRE(document.has_value());
    CHECK(*document == "Documents");

    const auto image = CategoryDateSuffix::strip_generated_suffix(
        "Images_2026-06-28", CategoryDateSuffix::Kind::Image);
    REQUIRE(image.has_value());
    CHECK(*image == "Images");

    CHECK_FALSE(CategoryDateSuffix::strip_generated_suffix(
        "Images_2026-06-28", CategoryDateSuffix::Kind::Document).has_value());
    CHECK_FALSE(CategoryDateSuffix::strip_generated_suffix(
        "Documents_2026", CategoryDateSuffix::Kind::Document).has_value());
}
