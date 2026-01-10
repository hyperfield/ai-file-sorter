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
