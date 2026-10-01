#include <catch2/catch_test_macros.hpp>

#include "FolderStructurePattern.hpp"
#include "FolderTreeCatalog.hpp"
#include "JohnnyDecimalArchiveIndex.hpp"
#include "JohnnyDecimalFolderSuggester.hpp"
#include "JohnnyDecimalValidator.hpp"
#include "TestHelpers.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

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

TEST_CASE("FolderTreeCatalog scores prefixed folders by human labels")
{
    CHECK(FolderTreeCatalog::semantic_match_score("10-19 Admin/11 Reports",
                                                  "Admin",
                                                  "Reports") >= 20);
    CHECK(FolderTreeCatalog::semantic_match_score("AC Documents",
                                                  "Documents",
                                                  "Invoices") >= 8);
}

TEST_CASE("FolderStructurePattern suggests missing child inside numbered parent")
{
    FolderTreeCatalog::Catalog catalog({
        {"20-29 Work", 1},
        {"20-29 Work/21 Clients", 2},
        {"20-29 Work/23 Meeting Notes", 2},
    });

    const auto suggestion =
        FolderStructurePattern::suggest_new_folder(catalog, "Work", "Proposals");

    REQUIRE(suggestion.has_value());
    CHECK(suggestion->relative_path == "20-29 Work/22 Proposals");
    CHECK(suggestion->high_confidence);
}

TEST_CASE("FolderStructurePattern suggests new Johnny Decimal-like area")
{
    FolderTreeCatalog::Catalog catalog({
        {"10-19 Admin", 1},
        {"10-19 Admin/11 Reports", 2},
        {"20-29 Work", 1},
        {"20-29 Work/21 Clients", 2},
        {"30-39 Personal", 1},
        {"30-39 Personal/31 Travel", 2},
    });

    const auto suggestion =
        FolderStructurePattern::suggest_new_folder(catalog, "Finance", "Invoices");

    REQUIRE(suggestion.has_value());
    CHECK(suggestion->relative_path == "40-49 Finance/41 Invoices");
    CHECK(suggestion->high_confidence);
}

TEST_CASE("JohnnyDecimalFolderSuggester previews the next child number")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Reports"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Clients"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "23 Meeting Notes"));

    std::string error;
    const auto suggestion =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "Work", "Proposals", {}, &error);

    REQUIRE(suggestion.has_value());
    CHECK(suggestion->relative_path == "20-29 Work/22 Proposals");
    CHECK(suggestion->absolute_path == temp_dir.path() / "20-29 Work" / "22 Proposals");
    CHECK(error.empty());
}

TEST_CASE("JohnnyDecimalFolderSuggester creates a new area and first child folder")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Reports"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Clients"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "30-39 Personal" / "31 Travel"));

    const auto result =
        JohnnyDecimalFolderSuggester::create_next_folder(temp_dir.path(), "Finance", "Invoices");

    REQUIRE(result.success);
    CHECK(result.suggestion.relative_path == "40-49 Finance/41 Invoices");
    CHECK(result.created_directories.size() == 2);
    CHECK(std::filesystem::is_directory(temp_dir.path() / "40-49 Finance"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "40-49 Finance" / "41 Invoices"));
}

TEST_CASE("JohnnyDecimalFolderSuggester rejects duplicate sibling labels")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Reports"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Clients"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "22 Proposals"));

    std::string error;
    const auto suggestion =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "Work", "Proposals", {}, &error);

    CHECK_FALSE(suggestion.has_value());
    CHECK(error.find("20-29 Work/22 Proposals") != std::string::npos);
}

TEST_CASE("JohnnyDecimalFolderSuggester rejects occupied next-number file paths")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Reports"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Clients"));
    std::ofstream(temp_dir.path() / "20-29 Work" / "22 Proposals").put('x');

    std::string error;
    const auto suggestion =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "Work", "Proposals", {}, &error);

    CHECK_FALSE(suggestion.has_value());
    CHECK(error.find("already exists") != std::string::npos);
}

TEST_CASE("JohnnyDecimalFolderSuggester requires a Johnny Decimal-like archive")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "Work" / "Clients"));

    std::string error;
    const auto suggestion =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "Work", "Proposals", {}, &error);

    CHECK_FALSE(suggestion.has_value());
    CHECK(error.find("Johnny.Decimal-like") != std::string::npos);
}

TEST_CASE("JohnnyDecimalValidator accepts a clean Johnny Decimal archive")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "00-09 System" / "01 Index"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Finance"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Projects"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Projects" / "21.01 Alpha"));

    const auto report = JohnnyDecimalValidator::validate_archive(temp_dir.path());

    CHECK(report.scanned);
    CHECK(report.looks_johnny_decimal_like);
    CHECK(report.area_count == 3);
    CHECK(report.category_count == 3);
    CHECK(report.issues.empty());
    CHECK_FALSE(report.has_errors());

    const std::string text = JohnnyDecimalValidator::format_report(report);
    CHECK(text.find("No issues found") != std::string::npos);
}

TEST_CASE("JohnnyDecimalValidator reports duplicate malformed and outside-range folders")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Finance"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Legal"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Projects"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "37 Marketing"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work Duplicate" / "22 Clients"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "30-39 Personal" / "3 Health"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "30-39 Personal" / "Misc Notes"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-30 Bad Range"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20_29 Bad Syntax"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "11 Loose Category"));

    const auto report = JohnnyDecimalValidator::validate_archive(temp_dir.path());

    std::set<std::string> codes;
    for (const auto& issue : report.issues) {
        codes.insert(issue.code);
    }

    CHECK(report.has_errors());
    CHECK(codes.contains("duplicate_area_range"));
    CHECK(codes.contains("duplicate_category_number"));
    CHECK(codes.contains("category_outside_area_range"));
    CHECK(codes.contains("category_id_malformed"));
    CHECK(codes.contains("missing_category_id"));
    CHECK(codes.contains("area_id_malformed"));
    CHECK(codes.contains("category_without_area"));

    const std::string text = JohnnyDecimalValidator::format_report(report);
    CHECK(text.find("ERROR - Duplicate area range") != std::string::npos);
    CHECK(text.find("20-29 Work/37 Marketing") != std::string::npos);
}

TEST_CASE("JohnnyDecimalValidator reports missing area structure")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "Work" / "Clients"));

    const auto report = JohnnyDecimalValidator::validate_archive(temp_dir.path());

    REQUIRE_FALSE(report.issues.empty());
    CHECK(report.area_count == 0);
    CHECK(report.category_count == 0);
    CHECK(report.has_errors());
    CHECK(report.issues.front().code == "missing_area_structure");
}

TEST_CASE("JohnnyDecimalArchiveIndex maps areas categories and items")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "00-09 System" / "01 Index" / "01.01 Inbox"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "00-09 System" / "01 Index" / "Notes"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "21 Projects" / "21.01 Alpha" / "Drafts"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "22 Reference"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work" / "Misc Unnumbered"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "Loose"));

    const auto index = JohnnyDecimalArchiveIndex::build_index(temp_dir.path());

    CHECK(index.scanned);
    CHECK(index.areas.size() == 2);
    CHECK(JohnnyDecimalArchiveIndex::category_count(index) == 3);
    CHECK(JohnnyDecimalArchiveIndex::item_count(index) == 4);
    REQUIRE(index.areas.size() >= 2);
    CHECK(index.areas[1].other_direct_children == std::vector<std::string>{"20-29 Work/Misc Unnumbered"});
    CHECK(index.other_top_level == std::vector<std::string>{"Loose"});

    const std::string text = JohnnyDecimalArchiveIndex::format_index(index);
    CHECK(text.find("00-09 System") != std::string::npos);
    CHECK(text.find("01 Index") != std::string::npos);
    CHECK(text.find("21.01 Alpha") != std::string::npos);
    CHECK(text.find("Drafts") != std::string::npos);
    CHECK(text.find("Other direct folders") != std::string::npos);
    CHECK(text.find("Misc Unnumbered") != std::string::npos);
    CHECK(text.find("Other top-level folders") != std::string::npos);
    CHECK(text.find("Loose") != std::string::npos);
}

TEST_CASE("FolderStructurePattern nests under matching custom code folders")
{
    FolderTreeCatalog::Catalog catalog({
        {"AA Images", 1},
        {"AB Photos", 1},
        {"AC Documents", 1},
        {"DG Office Apps", 1},
    });

    const auto documents =
        FolderStructurePattern::suggest_new_folder(catalog, "Documents", "Invoices");
    REQUIRE(documents.has_value());
    CHECK(documents->relative_path == "AC Documents/Invoices");
    CHECK(documents->high_confidence);

    const auto programs =
        FolderStructurePattern::suggest_new_folder(catalog, "Programs", "Installers");
    REQUIRE(programs.has_value());
    CHECK(programs->relative_path == "DG Office Apps/Installers");
    CHECK(programs->high_confidence);
}

TEST_CASE("FolderTreeCatalog prompt includes detected structure conventions")
{
    FolderTreeCatalog::Catalog catalog({
        {"10-19 Admin", 1},
        {"10-19 Admin/11 Reports", 2},
        {"20-29 Work", 1},
        {"20-29 Work/21 Clients", 2},
    });

    const std::string prompt = FolderTreeCatalog::build_prompt_context(
        catalog,
        "budget.xlsx",
        "D:/Incoming/budget.xlsx",
        true,
        "Finance",
        "Budgets",
        "Finance/Budgets");

    CHECK(prompt.find("Detected folder structure conventions") != std::string::npos);
    CHECK(prompt.find("Johnny.Decimal-like") != std::string::npos);
    CHECK(prompt.find("Convention-aware deterministic new-folder candidate: 30-39 Finance/31 Budgets") !=
          std::string::npos);
}

TEST_CASE("FolderTreeCatalog derives compatibility labels from target path")
{
    const auto labels =
        FolderTreeCatalog::derive_category_pair("10-19 Admin/11 Reports/11.04 Tax summaries");

    CHECK(labels.first == "10-19 Admin");
    CHECK(labels.second == "11.04 Tax summaries");
}
