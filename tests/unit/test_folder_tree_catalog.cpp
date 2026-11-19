#include <catch2/catch_test_macros.hpp>

#include "FolderStructurePattern.hpp"
#include "FolderStructurePluginProfile.hpp"
#include "FolderTreeCatalog.hpp"
#include "FolderTreeRoutingExplanation.hpp"
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

TEST_CASE("FolderTreeCatalog reads complete direct sibling inventories safely", "[folder-allocation]")
{
    TempDir root;
    constexpr int kSiblingCount = 510;
    for (int i = 0; i < kSiblingCount; ++i) {
        std::filesystem::create_directories(root.path() / "Parent" / ("Item " + std::to_string(i)));
    }
    std::filesystem::create_directories(root.path() / "Parent/Item 0/Nested");
    std::filesystem::create_directories(root.path() / "Parent/.ignored");
    const auto siblings = FolderTreeCatalog::scan_direct_children(root.path(), "Parent");
    REQUIRE(siblings);
    CHECK(siblings->entries().size() == kSiblingCount);
    CHECK(siblings->find_existing("Parent/Item 509").has_value());
    CHECK_FALSE(siblings->find_existing("Parent/Item 0/Nested").has_value());
    CHECK_FALSE(siblings->find_existing("Parent/.ignored").has_value());
    CHECK(siblings->entries().front().depth == 2);
    const auto top = FolderTreeCatalog::scan_direct_children(root.path());
    REQUIRE(top);
    CHECK(top->entries().size() == 1);
    CHECK(top->find_existing("Parent").has_value());
    const auto pending = FolderTreeCatalog::scan_direct_children(root.path(), "Uncreated/Parent");
    REQUIRE(pending);
    CHECK(pending->empty());
    CHECK_FALSE(FolderTreeCatalog::scan_direct_children(root.path(), "../outside"));
    CHECK_FALSE(FolderTreeCatalog::scan_direct_children(root.path() / "missing"));
    std::ofstream(root.path() / "blocked") << "file";
    std::string error;
    CHECK_FALSE(FolderTreeCatalog::scan_direct_children(root.path(), "blocked", &error));
    CHECK_FALSE(error.empty());
    TempDir outside;
    std::error_code symlink_error;
    std::filesystem::create_directory_symlink(outside.path(), root.path() / "escape", symlink_error);
    if (!symlink_error) CHECK_FALSE(FolderTreeCatalog::scan_direct_children(root.path(), "escape"));
}

TEST_CASE("FolderStructurePattern inventories siblings of newly allocated parents", "[folder-allocation]")
{
    TempDir root;
    std::filesystem::create_directories(root.path() / "10-19 Finance/11 Invoices");
    constexpr int kLargeFolderCount = 510;
    for (int i = 0; i < kLargeFolderCount; ++i) {
        std::filesystem::create_directories(root.path() / "20-29 Work/21 Projects/21.01 Current" /
                                             ("Notes " + std::to_string(i)));
    }
    std::filesystem::create_directories(root.path() / "30-39 Reference/31 Books");
    std::filesystem::create_directories(root.path() / "90-99 Archive/91 Old");
    const auto catalog = FolderTreeCatalog::Catalog::scan(root.path());
    std::vector<std::string> parents;
    const auto allocated = FolderStructurePattern::allocate_proposed_path(catalog, "30-39 Research/Books",
        [&](std::string_view parent) -> std::optional<std::vector<FolderTreeCatalog::Entry>> {
            parents.emplace_back(parent);
            const auto siblings = FolderTreeCatalog::scan_direct_children(root.path(), parent);
            return siblings ? std::optional(siblings->entries()) : std::nullopt;
        });
    REQUIRE(allocated);
    CHECK(*allocated == "40-49 Research/41 Books");
    CHECK(parents == std::vector<std::string>{"", "40-49 Research"});
    CHECK_FALSE(std::filesystem::exists(root.path() / "40-49 Research"));
    CHECK_FALSE(FolderStructurePattern::allocate_proposed_path(catalog, "20-29 Work/Proposals",
        [](std::string_view) -> std::optional<std::vector<FolderTreeCatalog::Entry>> { return std::nullopt; }));
}

TEST_CASE("FolderTreeCatalog validates safe relative folder paths")
{
    const auto valid = FolderTreeCatalog::validate_relative_folder_path("10-19 Admin\\11 Reports");
    REQUIRE(valid.valid);
    CHECK(valid.normalized_path == "10-19 Admin/11 Reports");

    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("../escape").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("/escape").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("\\\\server\\share").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("C:/escape").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("Admin/<bad>").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("Admin/CON.txt").valid);
    CHECK(FolderTreeCatalog::validate_relative_folder_path("Media/75 Økonomi ☁️").valid);
    CHECK(FolderTreeCatalog::validate_relative_folder_path("Media/75 Cafe\xcc\x81").valid);
    CHECK(FolderTreeCatalog::validate_relative_folder_path("Media/75 Ġ").valid);
    CHECK_FALSE(FolderTreeCatalog::validate_relative_folder_path("Media/Bad\x01").valid);
}

TEST_CASE("FolderTreeCatalog validates destination depth relative to the selected root", "[destination-depth]")
{
    using namespace FolderTreeCatalog;
    TempDir root;
    std::filesystem::path relative;
    for (int i = 1; i <= kDefaultMaxDepth; ++i) relative /= "Level " + std::to_string(i);
    std::filesystem::create_directories(root.path() / relative);
    // Files at the next level do not add another directory level.
    std::ofstream(root.path() / relative / "notes.txt") << "fixture";
    CHECK(validate_depth(root.path()).status == DepthValidationStatus::Valid);
    CHECK(depth_validation_message(validate_depth(root.path())).empty());

    relative /= "Too deep";
    std::filesystem::create_directories(root.path() / relative / "Even deeper");
    const auto result = validate_depth(root.path());
    CHECK(result.status == DepthValidationStatus::TooDeep);
    CHECK(result.relative_path == Utils::path_to_utf8(relative.generic_string()));
    CHECK(depth_validation_message(result).find("Too deep") != std::string::npos);
    CHECK(depth_validation_message(result).find("Even deeper") == std::string::npos);
    // A more specific selected destination has its own depth zero.
    CHECK(validate_depth(root.path() / "Level 1/Level 2").status == DepthValidationStatus::Valid);
}

TEST_CASE("FolderTreeCatalog depth validation ignores the routing entry budget", "[destination-depth]")
{
    TempDir root;
    constexpr int kWideFolderCount = 510;
    for (int i = 0; i < kWideFolderCount; ++i) {
        std::filesystem::create_directories(root.path() / "A Shallow" / ("Folder " + std::to_string(i)));
    }
    std::filesystem::path deep = "Z Deep";
    for (int i = 1; i <= FolderTreeCatalog::kDefaultMaxDepth; ++i) deep /= "Level " + std::to_string(i);
    std::filesystem::create_directories(root.path() / deep);
    const auto result = FolderTreeCatalog::validate_depth(root.path());
    CHECK(result.status == FolderTreeCatalog::DepthValidationStatus::TooDeep);
    CHECK(result.relative_path == Utils::path_to_utf8(deep.generic_string()));
}

TEST_CASE("FolderTreeCatalog depth validation respects excluded folders and cancellation", "[destination-depth]")
{
    using namespace FolderTreeCatalog;
    TempDir root;
    std::filesystem::path relative = ".hidden";
    for (int i = 0; i <= kDefaultMaxDepth; ++i) relative /= "Nested";
    std::filesystem::create_directories(root.path() / relative);
    CHECK(validate_depth(root.path()).status == DepthValidationStatus::Valid);
    CHECK(validate_depth(root.path() / "missing").status == DepthValidationStatus::Failed);
    CHECK_FALSE(depth_validation_message(validate_depth(root.path() / "missing")).empty());
    CHECK(validate_depth(root.path(), [] { return true; }).status == DepthValidationStatus::Cancelled);
    for (int i = 0; i < kDefaultMaxDepth; ++i) {
        std::filesystem::create_directories(root.path() / ("Folder " + std::to_string(i)));
    }
    int probes = 0;
    const auto cancelled = validate_depth(root.path(), [&] { return ++probes > 3; });
    CHECK(cancelled.status == DepthValidationStatus::Cancelled);
    CHECK(depth_validation_message(cancelled).empty());
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

TEST_CASE("FolderTreeCatalog does not reinterpret rejected JSON as permission to create")
{
    const FolderTreeCatalog::Catalog catalog({{"70-79 Media & Design", 1},
        {"70-79 Media & Design/74 Presentations", 2}});
    for (const auto* reply : {
        R"({"targetFolder":"40-79 Media & Design/74 Presentations","createFolder":false})",
        R"({"targetFolder":"36-79 Media & Design/74 Presentations","createFolder":false})",
        R"({"targetFolder":"70-79 Media & Design/75 New","createFolder":"false"})",
        R"({"targetFolder":"70-79 Media & Design/75 New","createFolder":true,"newFolder":false})"}) {
        CHECK_FALSE(FolderTreeCatalog::parse_selection(reply, catalog, true));
        CHECK_FALSE(FolderTreeCatalog::parse_selection(std::string("```json\n") + reply + "\n```", catalog, true));
        CHECK_FALSE(FolderTreeCatalog::parse_selection(std::string("Answer: ") + reply, catalog, true));
    }
    CHECK(FolderTreeCatalog::parse_selection(
        R"({"targetFolder":"70-79 Media & Design/75 New","createFolder":true})", catalog, true));
    for (const auto* reply : {
        R"("targetFolder":"40-79 Media & Design/74 Presentations","createFolder":false)",
        R"(40-79 Media & Design/74 Presentations","createFolder : false)",
        R"("targetFolder":"70-79 Media & Design/75 New","createFolder":true,"newFolder":false)",
        R"("targetFolder":"70-79 Media & Design/75 New","createFolder":unknown)"}) {
        CHECK_FALSE(FolderTreeCatalog::parse_selection(reply, catalog, true));
    }
}

TEST_CASE("FolderTreeCatalog plain paths retain validation and creation permissions", "[plain-path]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance", 1}, {"Finance/Invoices", 2}});
    for (const bool allow_new : {false, true}) {
        const auto existing = FolderTreeCatalog::parse_selection("finance/invoices", catalog, allow_new);
        REQUIRE(existing);
        CHECK(existing->relative_path == "Finance/Invoices");
        CHECK(existing->exists);
        CHECK_FALSE(existing->suggested_new);
        const auto proposed = FolderTreeCatalog::parse_selection("Finance/Forecasts", catalog, allow_new);
        CHECK(proposed.has_value() == allow_new);
        if (proposed) {
            CHECK_FALSE(proposed->exists);
            CHECK(proposed->suggested_new);
        }
        for (const auto* unsafe : {"../outside", "/outside", "\\\\server\\share", "C:/outside", "Finance/../outside",
                                   "Finance/<invalid>", "Finance/CON.txt"}) {
            INFO("Target: " << unsafe << "; new folders: " << allow_new);
            CHECK_FALSE(FolderTreeCatalog::parse_selection(unsafe, catalog, allow_new));
        }
    }
}

TEST_CASE("FolderTreeCatalog proposal prompt describes only the proposal stage")
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

    CHECK(prompt.find("New destination proposal:") != std::string::npos);
    CHECK(prompt.find("createFolder:false") == std::string::npos);
    CHECK(prompt.find("createFolder:true") != std::string::npos);
    CHECK(prompt.find("weak, generic") == std::string::npos);
    CHECK(prompt.find("catch-all") == std::string::npos);
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

TEST_CASE("FolderTreeCatalog omits creation guidance while preserving existing numbered paths", "[folder-response]")
{
    const FolderTreeCatalog::Catalog catalog({
        {"10-19 Admin", 1}, {"10-19 Admin/11 Finance", 2},
        {"20-29 Work", 1}, {"20-29 Work/21 Projects", 2}});
    FolderStructurePluginProfile plugin;
    plugin.id = "jd";
    plugin.name = "Johnny.Decimal";
    plugin.structure_kind = "johnny_decimal";
    plugin.prompt_guidance = "Read the label after the Johnny.Decimal ID.\n";
    plugin.new_folder_guidance = "Allocate the next free ID for a new child.\n";
    for (const bool proposal : {false, true}) {
        const auto context = FolderTreeCatalog::build_candidate_prompt_context(
            catalog, catalog.entries(), proposal, "Finance", "Budgets", std::span(&plugin, 1));
        CHECK(context.find(plugin.prompt_guidance) != std::string::npos);
        CHECK((context.find(plugin.new_folder_guidance) != std::string::npos) == proposal);
        CHECK((context.find("proposing a missing folder") != std::string::npos) == proposal);
        CHECK((context.find("Detected folder structure conventions") != std::string::npos) == proposal);
        CHECK(context.find("20-29 Work/21 Projects\n") != std::string::npos);
        CHECK(context.find("1. 10-19") == std::string::npos);
        CHECK((context.find("createFolder:true") != std::string::npos) == proposal);
        CHECK(context.find("createFolder:false") == std::string::npos);
        CHECK(context.find("weak, generic") == std::string::npos);
    }
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

TEST_CASE("FolderStructurePattern preserves range width when allocating the first child")
{
    std::string area = "00-09 System";
    std::string child = "01 Notes";
    SECTION("Two-digit range") {}
    SECTION("Wider padded range") {
        area = "000-009 System";
        child = "001 Notes";
    }
    SECTION("Unpadded range") {
        area = "0-9 System";
        child = "1 Notes";
    }
    const FolderTreeCatalog::Catalog catalog({{area, 1}});
    const std::string expected = area + "/" + child;

    const auto suggestion = FolderStructurePattern::suggest_new_folder(catalog, "System", "Notes");
    REQUIRE(suggestion.has_value());
    CHECK(suggestion->relative_path == expected);

    const auto allocated = FolderStructurePattern::allocate_proposed_path(catalog, area + "/Notes");
    REQUIRE(allocated.has_value());
    CHECK(*allocated == expected);
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

TEST_CASE("JohnnyDecimalFolderSuggester preserves two-digit IDs in an empty System area")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "00-09 System"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin/11 Finance"));
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "20-29 Work/21 Projects"));

    std::string error;
    const auto preview =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "System", "Notes", {}, &error);
    INFO(error);
    REQUIRE(preview.has_value());
    CHECK(preview->relative_path == "00-09 System/01 Notes");
    CHECK_FALSE(std::filesystem::exists(preview->absolute_path));

    const auto result = JohnnyDecimalFolderSuggester::create_next_folder(temp_dir.path(), "System", "Notes");
    INFO(result.error);
    REQUIRE(result.success);
    CHECK(result.suggestion.relative_path == "00-09 System/01 Notes");
    CHECK(result.created_directories.size() == 1);
    CHECK(std::filesystem::is_directory(temp_dir.path() / "00-09 System/01 Notes"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.path() / "00-09 System/1 Notes"));

    const auto report = JohnnyDecimalValidator::validate_archive(temp_dir.path());
    INFO(JohnnyDecimalValidator::format_report(report));
    CHECK_FALSE(report.has_errors());

    const auto next =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "System", "Templates", {}, &error);
    REQUIRE(next.has_value());
    CHECK(next->relative_path == "00-09 System/02 Templates");
}

TEST_CASE("JohnnyDecimalFolderSuggester preserves occupied IDs in large archives")
{
    TempDir temp_dir;
    std::filesystem::create_directories(temp_dir.path() / "00-09 System/01 Index");
    std::filesystem::create_directories(temp_dir.path() / "10-19 Admin/11 Finance");
    std::filesystem::create_directories(temp_dir.path() / "20-29 Work/21 Projects/21.01 Current");

    auto populated_parent = temp_dir.path() / "20-29 Work/21 Projects/21.01 Current";
    SECTION("Large item subtree") {}
    SECTION("Large set of direct area children") {
        populated_parent = temp_dir.path() / "10-19 Admin";
    }
    constexpr int kLargeArchiveFolderCount = 510;
    for (int i = 0; i < kLargeArchiveFolderCount; ++i) {
        std::filesystem::create_directories(populated_parent / ("Notes " + std::to_string(i)));
    }
    std::filesystem::create_directories(temp_dir.path() / "20-29 Work/22 Existing");

    std::string error;
    const auto suggestion =
        JohnnyDecimalFolderSuggester::preview_next_folder(temp_dir.path(), "Work", "Proposals", {}, &error);
    INFO(error);
    REQUIRE(suggestion.has_value());
    CHECK(suggestion->relative_path == "20-29 Work/23 Proposals");
    CHECK_FALSE(std::filesystem::exists(suggestion->absolute_path));

    const auto result = JohnnyDecimalFolderSuggester::create_next_folder(temp_dir.path(), "Work", "Proposals");
    INFO(result.error);
    REQUIRE(result.success);
    CHECK(result.suggestion.relative_path == "20-29 Work/23 Proposals");
    CHECK(result.created_directories.size() == 1);
    CHECK(std::filesystem::is_directory(temp_dir.path() / "20-29 Work/22 Existing"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "20-29 Work/23 Proposals"));
    CHECK_FALSE(std::filesystem::exists(temp_dir.path() / "20-29 Work/22 Proposals"));
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

    SECTION("Small archive") {}
    SECTION("Large item subtree") {
        constexpr int kLargeArchiveFolderCount = 510;
        const auto item = temp_dir.path() / "20-29 Work/21 Clients/21.01 Current";
        for (int i = 0; i < kLargeArchiveFolderCount; ++i) {
            std::filesystem::create_directories(item / ("Notes " + std::to_string(i)));
        }
    }

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
    CHECK(prompt.find("Preliminary labels (optional clues): Finance / Budgets") != std::string::npos);
    CHECK(prompt.find("deterministic new-folder candidate") == std::string::npos);
}

TEST_CASE("FolderTreeCatalog derives compatibility labels from target path")
{
    const auto labels =
        FolderTreeCatalog::derive_category_pair("10-19 Admin/11 Reports/11.04 Tax summaries");

    CHECK(labels.first == "10-19 Admin");
    CHECK(labels.second == "11.04 Tax summaries");
}

TEST_CASE("FolderTreeRoutingExplanation describes Johnny Decimal destinations")
{
    FolderStructurePattern::Profile profile;
    profile.has_recognized_conventions = true;
    profile.has_johnny_decimal_like_ranges = true;
    profile.has_numbered_prefixes = true;

    const FolderTreeCatalog::Selection selection{
        "20-29 Work/22 Proposals",
        true,
        false
    };
    const FolderTreeCatalog::SemanticMatch best_existing{
        FolderTreeCatalog::Entry{"20-29 Work/21 Clients", 2},
        12
    };

    const std::string explanation = FolderTreeRoutingExplanation::build({
        "Work",
        "Proposals",
        "Work/Proposals",
        selection,
        best_existing,
        profile
    });

    CHECK(explanation.find("AI category: Work / Proposals.") != std::string::npos);
    CHECK(explanation.find("Semantic target: Work/Proposals.") != std::string::npos);
    CHECK(explanation.find("Johnny.Decimal-like") != std::string::npos);
    CHECK(explanation.find("Readable target: Work/Proposals.") != std::string::npos);
    CHECK(explanation.find("Closest existing folder: 20-29 Work/21 Clients (score 12).") !=
          std::string::npos);
    CHECK(explanation.find("Decision: suggest new target 20-29 Work/22 Proposals") !=
          std::string::npos);
}
