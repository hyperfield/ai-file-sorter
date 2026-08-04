#include <catch2/catch_test_macros.hpp>

#include "ReviewNameValidator.hpp"

#include <string>

TEST_CASE("ReviewNameValidator validates filenames")
{
    std::string error;

    CHECK(ReviewNameValidator::validate_filename("quarterly_report.txt", error));
    CHECK(ReviewNameValidator::validate_filename(
        std::string("rapport_") + "\xC3\xA9" + "nergie.txt",
        error));

    CHECK_FALSE(ReviewNameValidator::validate_filename("", error));
    CHECK(error == "Filename is empty");

    CHECK_FALSE(ReviewNameValidator::validate_filename("CON.txt", error));
    CHECK(error == "Filename is a reserved name");

    CHECK_FALSE(ReviewNameValidator::validate_filename("bad:name.txt", error));
    CHECK(error == "Filename contains disallowed characters");

    CHECK_FALSE(ReviewNameValidator::validate_filename("trailing-dot.", error));
    CHECK(error == "Filename has leading/trailing space or dot");
}

TEST_CASE("ReviewNameValidator validates category labels")
{
    std::string error;

    CHECK(ReviewNameValidator::validate_labels("Documents", "Reports", error));
    CHECK(ReviewNameValidator::validate_labels(
        std::string("Rapports ") + "\xC3\xA9" + "nergie",
        "Q2",
        error));

    CHECK_FALSE(ReviewNameValidator::validate_labels("Documents", "Documents", error));
    CHECK(error == "Category and subcategory are identical");

    CHECK(ReviewNameValidator::validate_labels("Documents", "Documents", error, true));

    CHECK_FALSE(ReviewNameValidator::validate_labels("pdf", "Reports.txt", error));
    CHECK(error == "Category or subcategory looks like a file extension");

    CHECK_FALSE(ReviewNameValidator::validate_labels("CON", "Reports", error));
    CHECK(error == "Category or subcategory is a reserved name");
}

TEST_CASE("ReviewNameValidator normalizes review labels")
{
    CHECK(ReviewNameValidator::trim_copy("  Reports  ") == "Reports");
    CHECK(ReviewNameValidator::is_missing_category_label(" Uncategorized "));
    CHECK(ReviewNameValidator::is_missing_category_label(""));
    CHECK_FALSE(ReviewNameValidator::is_missing_category_label("Documents"));

    CHECK(ReviewNameValidator::strip_history_description_label(
              "Image description:  mountain landscape ") ==
          "mountain landscape");
    CHECK(ReviewNameValidator::strip_history_description_label(
              "Document summary:  invoice details ") ==
          "invoice details");
    CHECK(ReviewNameValidator::strip_history_description_label("plain text") == "plain text");
}
