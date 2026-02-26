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

TEST_CASE("DatabaseManager normalizes backup category synonyms for taxonomy matching") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto archives = db.resolve_category("Archives", "General");
    auto backup = db.resolve_category("backup files", "General");

    REQUIRE(archives.taxonomy_id > 0);
    CHECK(backup.taxonomy_id == archives.taxonomy_id);
    CHECK(backup.category == archives.category);
    CHECK(backup.category == "Archives");
    CHECK(backup.subcategory == "General");
}

TEST_CASE("DatabaseManager normalizes image category synonyms and image media aliases") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto images = db.resolve_category("Images", "Photos");
    auto graphics = db.resolve_category("Graphics", "Photos");
    auto media_images = db.resolve_category("Media", "Photos");
    auto media_audio = db.resolve_category("Media", "Audio");

    REQUIRE(images.taxonomy_id > 0);
    CHECK(graphics.taxonomy_id == images.taxonomy_id);
    CHECK(media_images.taxonomy_id == images.taxonomy_id);
    CHECK(graphics.category == "Images");
    CHECK(media_images.category == "Images");

    CHECK(media_audio.category == "Media");
    CHECK(media_audio.taxonomy_id != images.taxonomy_id);
}

TEST_CASE("DatabaseManager normalizes document category synonyms for taxonomy matching") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto documents = db.resolve_category("Documents", "Reports");
    auto texts = db.resolve_category("Texts", "Reports");
    auto papers = db.resolve_category("Papers", "Reports");
    auto spreadsheets = db.resolve_category("Spreadsheets", "Reports");

    REQUIRE(documents.taxonomy_id > 0);
    CHECK(texts.taxonomy_id == documents.taxonomy_id);
    CHECK(papers.taxonomy_id == documents.taxonomy_id);
    CHECK(spreadsheets.taxonomy_id == documents.taxonomy_id);
    CHECK(texts.category == "Documents");
    CHECK(papers.category == "Documents");
    CHECK(spreadsheets.category == "Documents");
}

TEST_CASE("DatabaseManager normalizes installer and update category synonyms for taxonomy matching") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto software = db.resolve_category("Software", "Installers");
    auto installers = db.resolve_category("Installers", "Installers");
    auto setup_files = db.resolve_category("Setup files", "Installers");
    auto updates = db.resolve_category("Software Update", "Installers");
    auto patches = db.resolve_category("Patches", "Installers");

    REQUIRE(software.taxonomy_id > 0);
    CHECK(installers.taxonomy_id == software.taxonomy_id);
    CHECK(setup_files.taxonomy_id == software.taxonomy_id);
    CHECK(updates.taxonomy_id == software.taxonomy_id);
    CHECK(patches.taxonomy_id == software.taxonomy_id);
    CHECK(installers.category == "Software");
    CHECK(setup_files.category == "Software");
    CHECK(updates.category == "Software");
    CHECK(patches.category == "Software");
}
