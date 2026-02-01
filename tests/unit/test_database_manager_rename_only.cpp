#include <catch2/catch_test_macros.hpp>

#include "DatabaseManager.hpp"
#include "TestHelpers.hpp"

TEST_CASE("DatabaseManager keeps rename-only entries with empty labels") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    const std::string dir_path = "/sample";
    DatabaseManager::ResolvedCategory empty{0, "", ""};
    const std::string suggested_name = "rename_suggestion.png";

    REQUIRE(db.insert_or_update_file_with_categorization(
        "rename.png", "F", dir_path, empty, false, suggested_name, true));
    REQUIRE(db.insert_or_update_file_with_categorization(
        "empty.png", "F", dir_path, empty, false, std::string(), false));

    const auto removed = db.remove_empty_categorizations(dir_path);
    REQUIRE(removed.size() == 1);
    CHECK(removed.front().file_name == "empty.png");

    const auto entries = db.get_categorized_files(dir_path);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().file_name == "rename.png");
    CHECK(entries.front().rename_only);
    CHECK_FALSE(entries.front().rename_applied);
    CHECK(entries.front().suggested_name == suggested_name);
    CHECK(entries.front().category.empty());
    CHECK(entries.front().subcategory.empty());
}

TEST_CASE("DatabaseManager keeps suggestion-only entries with empty labels") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    const std::string dir_path = "/sample";
    DatabaseManager::ResolvedCategory empty{0, "", ""};
    const std::string suggested_name = "suggested_name.png";

    REQUIRE(db.insert_or_update_file_with_categorization(
        "suggested.png", "F", dir_path, empty, false, suggested_name, false));

    const auto removed = db.remove_empty_categorizations(dir_path);
    CHECK(removed.empty());

    const auto entries = db.get_categorized_files(dir_path);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().file_name == "suggested.png");
    CHECK_FALSE(entries.front().rename_only);
    CHECK(entries.front().suggested_name == suggested_name);
    CHECK(entries.front().category.empty());
    CHECK(entries.front().subcategory.empty());
}

TEST_CASE("DatabaseManager normalizes subcategory stopword suffixes for taxonomy matching") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto base = db.resolve_category("Images", "Graphics");
    auto with_suffix = db.resolve_category("Images", "Graphics files");

    REQUIRE(base.taxonomy_id > 0);
    CHECK(with_suffix.taxonomy_id == base.taxonomy_id);
    CHECK(with_suffix.category == base.category);
    CHECK(with_suffix.subcategory == base.subcategory);

    auto photos = db.resolve_category("Images", "Photos");
    CHECK(photos.subcategory == "Photos");
}
