#include <catch2/catch_test_macros.hpp>

#include "FolderTreeCatalog.hpp"
#include "TestHelpers.hpp"

#include <filesystem>

TEST_CASE("FolderTreeCatalog scans relative destination folders")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Reports"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "22 Clients"));

    const auto catalog = FolderTreeCatalog::Catalog::scan(temp_dir.path());

    CHECK(catalog.find_existing("10-19 Admin").has_value());
    CHECK(catalog.find_existing("10-19 Admin/11 Reports").value_or("") ==
          "10-19 Admin/11 Reports");
    CHECK(catalog.find_existing("20-29 Work/22 Clients").value_or("") ==
          "20-29 Work/22 Clients");
}

TEST_CASE("FolderTreeCatalog validates safe relative folder paths")
{
    const auto valid = FolderTreeCatalog::validate_relative_folder_path("10-19 Admin\\11 Reports");
    REQUIRE(valid.valid);
    CHECK(valid.normalized_path == "10-19 Admin/11 Reports");

    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("../escape").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("C:/escape").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("Admin/<bad>").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("Admin/CON.txt").valid);
}

TEST_CASE("FolderTreeCatalog parses existing-only model responses")
{
    FolderTreeCatalog::Catalog catalog({
        {"10-19 Admin", 1},
        {"10-19 Admin/11 Reports", 2},
        {"10-19 Admin/11 Finance", 2},
        {"30-39 Personal/32 Health", 2},
    });

    const auto selection = FolderTreeCatalog::parse_selection(
        "{\"targetFolder\":\"10-19 admin/11 reports\",\"createFolder\":false}",
        catalog,
        false);

    REQUIRE(selection.has_value());
    CHECK(selection->relative_path == "10-19 Admin/11 Reports");
    CHECK(selection->exists);
    CHECK_FALSE(selection->suggested_new);

    CHECK_FALSE(FolderTreeCatalog::parse_selection(
                    "{\"targetFolder\":\"10-19 Admin/12 Receipts\",\"createFolder\":true}",
                    catalog,
                    false)
                    .has_value());

    const auto fenced = FolderTreeCatalog::parse_selection(
        "```json\n{\"targetFolder\":\"10-19 Admin/11 Finance\",\"createFolder\":false}\n```",
        catalog,
        false);
    REQUIRE(fenced.has_value());
    CHECK(fenced->relative_path == "10-19 Admin/11 Finance");

    const auto fragment = FolderTreeCatalog::parse_selection(
        "30-39 Personal/32 Health\",\"createFolder : false",
        catalog,
        false);
    REQUIRE(fragment.has_value());
    CHECK(fragment->relative_path == "30-39 Personal/32 Health");

    const auto unbraced_json = FolderTreeCatalog::parse_selection(
        "\"targetFolder\":\"10-19 Admin/11 Finance\",\"createFolder\":false",
        catalog,
        false);
    REQUIRE(unbraced_json.has_value());
    CHECK(unbraced_json->relative_path == "10-19 Admin/11 Finance");
}

TEST_CASE("FolderTreeCatalog accepts new folders only when enabled")
{
    FolderTreeCatalog::Catalog catalog({{"10-19 Admin", 1}});

    const auto selection = FolderTreeCatalog::parse_selection(
        "Target folder: 10-19 Admin/12 Receipts",
        catalog,
        true);

    REQUIRE(selection.has_value());
    CHECK(selection->relative_path == "10-19 Admin/12 Receipts");
    CHECK_FALSE(selection->exists);
    CHECK(selection->suggested_new);
}

TEST_CASE("FolderTreeCatalog prompt guides new folders over weak fallbacks")
{
    FolderTreeCatalog::Catalog catalog({
        {"Images", 1},
        {"Images/Work Screenshots", 2},
        {"Data and Archives", 1},
        {"Data and Archives/Compressed Archives", 2},
        {"Other", 1},
        {"Other/Unsorted Review", 2},
    });

    const std::string prompt = FolderTreeCatalog::build_prompt_context(
        catalog,
        "invoice_q2_2026.pdf",
        "D:/Incoming/invoice_q2_2026.pdf",
        true);

    CHECK(prompt.find("{\"targetFolder\":\"existing/folder/path\",\"createFolder\":false}") !=
          std::string::npos);
    CHECK(prompt.find("{\"targetFolder\":\"new/folder/path\",\"createFolder\":true}") !=
          std::string::npos);
    CHECK(prompt.find("listed candidates are not exhaustive") != std::string::npos);
    CHECK(prompt.find("weak, generic, or unrelated matches") != std::string::npos);
    CHECK(prompt.find("Do not choose fallback folders") != std::string::npos);
    CHECK(prompt.find("Other/Unsorted Review") != std::string::npos);
}

TEST_CASE("FolderTreeCatalog derives compatibility labels from target path")
{
    const auto labels =
        FolderTreeCatalog::derive_category_pair("10-19 Admin/11 Reports/11.04 Tax summaries");

    CHECK(labels.first == "10-19 Admin");
    CHECK(labels.second == "11.04 Tax summaries");
}
