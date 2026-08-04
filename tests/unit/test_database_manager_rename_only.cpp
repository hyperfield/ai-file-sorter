#include <catch2/catch_test_macros.hpp>

#include "DatabaseManager.hpp"
#include "TestHelpers.hpp"

#include <sqlite3.h>

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

TEST_CASE("DatabaseManager sanitizes invalid UTF-8 in cached labels") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    const std::string dir_path = "/sample";
    std::string invalid_category = "Imag";
    invalid_category.push_back(static_cast<char>(0xFF));
    invalid_category += "es";
    std::string invalid_subcategory = "Phot";
    invalid_subcategory.push_back(static_cast<char>(0xFF));
    invalid_subcategory += "os";
    std::string invalid_suggested = "sum";
    invalid_suggested.push_back(static_cast<char>(0xFF));
    invalid_suggested += "mer.png";

    DatabaseManager::ResolvedCategory resolved{
        0,
        invalid_category,
        invalid_subcategory,
    };

    REQUIRE(db.insert_or_update_file_with_categorization(
        "photo.png", "F", dir_path, resolved, false, invalid_suggested, false));

    const auto entries = db.get_categorized_files(dir_path);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().category == "Images");
    CHECK(entries.front().subcategory == "Photos");
    CHECK(entries.front().suggested_name == "summer.png");
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

TEST_CASE("DatabaseManager preserves the Backups family under archive-like labels") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto archives_backups = db.resolve_category("Archives", "Backups");
    auto backup = db.resolve_category("backup files", "General");

    REQUIRE(archives_backups.taxonomy_id > 0);
    CHECK(backup.taxonomy_id == archives_backups.taxonomy_id);
    CHECK(backup.category == "Backups");
    CHECK(backup.subcategory == "General");
    CHECK(archives_backups.category == "Backups");
    CHECK(archives_backups.subcategory == "General");
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

TEST_CASE("DatabaseManager canonicalizes legacy Music categories into Audio") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto audio = db.resolve_category("Audio", "Podcast");
    auto music = db.resolve_category("Music", "Podcast");

    REQUIRE(audio.taxonomy_id > 0);
    CHECK(music.taxonomy_id == audio.taxonomy_id);
    CHECK(music.category == "Audio");
    CHECK(music.subcategory == "Podcast");
}

TEST_CASE("DatabaseManager canonicalizes Installer Builders subcategories into Installer Tools") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto canonical = db.resolve_category("Software", "Installer Tools");
    auto legacy = db.resolve_category("Software", "Installer Builders");

    REQUIRE(canonical.taxonomy_id > 0);
    CHECK(legacy.taxonomy_id == canonical.taxonomy_id);
    CHECK(legacy.category == "Software");
    CHECK(legacy.subcategory == "Installer Tools");
}

TEST_CASE("DatabaseManager normalizes document category synonyms for taxonomy matching") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto documents = db.resolve_category("Documents", "Research");
    auto texts = db.resolve_category("Texts", "Research");
    auto papers = db.resolve_category("Papers", "Research");

    REQUIRE(documents.taxonomy_id > 0);
    CHECK(texts.taxonomy_id == documents.taxonomy_id);
    CHECK(papers.taxonomy_id == documents.taxonomy_id);
    CHECK(documents.category == "Documents");
    CHECK(texts.category == "Documents");
    CHECK(papers.category == "Documents");
}

TEST_CASE("DatabaseManager normalizes generic Documents labels into preserved document families when the subcategory is explicit") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto documents_manuals = db.resolve_category("Documents", "Manuals");
    auto manuals_general = db.resolve_category("Manuals", "General");
    auto documents_spreadsheets = db.resolve_category("Documents", "Spreadsheets");
    auto spreadsheets_general = db.resolve_category("Spreadsheets", "General");

    REQUIRE(documents_manuals.taxonomy_id > 0);
    CHECK(documents_manuals.taxonomy_id == manuals_general.taxonomy_id);
    CHECK(documents_manuals.category == "Manuals");
    CHECK(documents_manuals.subcategory == "General");

    REQUIRE(documents_spreadsheets.taxonomy_id > 0);
    CHECK(documents_spreadsheets.taxonomy_id == spreadsheets_general.taxonomy_id);
    CHECK(documents_spreadsheets.category == "Spreadsheets");
    CHECK(documents_spreadsheets.subcategory == "General");
}

TEST_CASE("DatabaseManager keeps specialized document-family categories and normalizes generic subcategories") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto documents_manuals = db.resolve_category("Documents", "Manuals");
    auto manuals_general = db.resolve_category("Manuals", "General");
    auto manuals_empty = db.resolve_category("Manuals", "");
    auto guides_general = db.resolve_category("Guides", "General");

    REQUIRE(documents_manuals.taxonomy_id > 0);
    CHECK(manuals_general.taxonomy_id == documents_manuals.taxonomy_id);
    CHECK(manuals_empty.taxonomy_id == documents_manuals.taxonomy_id);
    CHECK(manuals_general.category == "Manuals");
    CHECK(manuals_empty.category == "Manuals");
    CHECK(manuals_general.subcategory == "General");
    CHECK(manuals_empty.subcategory == "General");

    CHECK(guides_general.category == "Guides");
    CHECK(guides_general.subcategory == "General");
    CHECK(guides_general.taxonomy_id != documents_manuals.taxonomy_id);
}

TEST_CASE("DatabaseManager structurally canonicalizes broad main labels without reclassifying software semantics") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto installer = db.resolve_category("Installer", "Installer");
    auto operating_system = db.resolve_category("Operating System", "Operating System");
    auto software_installers = db.resolve_category("Software", "Installers");

    REQUIRE(installer.taxonomy_id > 0);
    CHECK(installer.category == "Installers");
    CHECK(installer.subcategory == "General");
    CHECK(operating_system.category == "Operating Systems");
    CHECK(operating_system.subcategory == "General");
    CHECK(software_installers.category == "Software");
    CHECK(software_installers.subcategory == "Installers");
    CHECK(software_installers.taxonomy_id != installer.taxonomy_id);
}

TEST_CASE("DatabaseManager keeps non-family software semantics under Software") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    auto updates = db.resolve_category("Software Update", "General");
    auto patches = db.resolve_category("Patches", "General");

    REQUIRE(updates.taxonomy_id > 0);
    CHECK(updates.category == "Software");
    CHECK(patches.category == "Software");
    CHECK(patches.taxonomy_id == updates.taxonomy_id);
}

TEST_CASE("DatabaseManager can clear cached categorizations together with taxonomy state") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    const std::string db_path = (base_dir.path() / "categorization_results.db").string();
    DatabaseManager db(base_dir.path().string());

    const auto resolved = db.resolve_category("Documents_2025-05", "Financial Documents");
    REQUIRE(resolved.taxonomy_id > 0);
    REQUIRE(db.insert_or_update_file_with_categorization("receipt.pdf",
                                                         "F",
                                                         "/sample",
                                                         resolved,
                                                         false,
                                                         std::string(),
                                                         false));

    REQUIRE(db.clear_all_categorizations(true));

    sqlite3* raw_db = nullptr;
    REQUIRE(sqlite3_open(db_path.c_str(), &raw_db) == SQLITE_OK);

    const auto count_rows = [&](const char* sql) {
        sqlite3_stmt* stmt = nullptr;
        REQUIRE(sqlite3_prepare_v2(raw_db, sql, -1, &stmt, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
        const int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return count;
    };

    CHECK(count_rows("SELECT COUNT(*) FROM file_categorization;") == 0);
    CHECK(count_rows("SELECT COUNT(*) FROM category_taxonomy;") == 0);
    CHECK(count_rows("SELECT COUNT(*) FROM category_alias;") == 0);
    CHECK(count_rows("SELECT COUNT(*) FROM category_translation;") == 0);

    sqlite3_close(raw_db);
}

TEST_CASE("DatabaseManager strips inline subcategory artifacts from stored translations") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    DatabaseManager db(base_dir.path().string());

    const auto resolved = db.resolve_category("Documents", "French Language Files");
    REQUIRE(resolved.taxonomy_id > 0);
    REQUIRE(db.upsert_category_translation(resolved.taxonomy_id,
                                           CategoryLanguage::French,
                                           "Documents , subcategory Fichiers en Francais",
                                           "Fichiers en Francais"));

    const auto translated = db.get_category_translation(resolved.taxonomy_id, CategoryLanguage::French);
    REQUIRE(translated.has_value());
    CHECK(translated->category == "Documents");
    CHECK(translated->subcategory == "Fichiers en Francais");

    const auto localized = db.localize_category(resolved, CategoryLanguage::French);
    CHECK(localized.category == "Documents");
    CHECK(localized.subcategory == "Fichiers en Francais");
}

TEST_CASE("DatabaseManager migrates legacy audio and installer-builder taxonomy labels on reopen") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    const std::string db_path = (base_dir.path() / "categorization_results.db").string();

    {
        DatabaseManager db(base_dir.path().string());
    }

    sqlite3* raw_db = nullptr;
    REQUIRE(sqlite3_open(db_path.c_str(), &raw_db) == SQLITE_OK);

    const auto exec_sql = [&](const char* sql) {
        char* error = nullptr;
        INFO(sql);
        REQUIRE(sqlite3_exec(raw_db, sql, nullptr, nullptr, &error) == SQLITE_OK);
        if (error) {
            sqlite3_free(error);
        }
    };

    exec_sql("DELETE FROM category_alias;");
    exec_sql("DELETE FROM category_translation;");
    exec_sql("DELETE FROM file_categorization;");
    exec_sql("DELETE FROM category_taxonomy;");
    exec_sql("INSERT INTO category_taxonomy "
             "(id, canonical_category, canonical_subcategory, normalized_category, normalized_subcategory, frequency) "
             "VALUES (1, 'Music', 'Podcast', 'music', 'podcast', 1);");
    exec_sql("INSERT INTO category_taxonomy "
             "(id, canonical_category, canonical_subcategory, normalized_category, normalized_subcategory, frequency) "
             "VALUES (2, 'Software', 'Installer Builders', 'software', 'installer builders', 1);");
    exec_sql("INSERT INTO file_categorization "
             "(file_name, file_type, dir_path, category, subcategory, taxonomy_id, categorization_style, rename_only, rename_applied) "
             "VALUES ('episode.mp3', 'F', '/sample', 'Music', 'Podcast', 1, 0, 0, 0);");
    exec_sql("INSERT INTO file_categorization "
             "(file_name, file_type, dir_path, category, subcategory, taxonomy_id, categorization_style, rename_only, rename_applied) "
             "VALUES ('builder.zip', 'F', '/sample', 'Software', 'Installer Builders', 2, 0, 0, 0);");
    REQUIRE(sqlite3_close(raw_db) == SQLITE_OK);

    {
        DatabaseManager db(base_dir.path().string());

        const auto episode = db.get_categorized_file("/sample", "episode.mp3", FileType::File);
        REQUIRE(episode.has_value());
        CHECK(episode->category == "Audio");
        CHECK(episode->subcategory == "Podcast");

        const auto builder = db.get_categorized_file("/sample", "builder.zip", FileType::File);
        REQUIRE(builder.has_value());
        CHECK(builder->category == "Software");
        CHECK(builder->subcategory == "Installer Tools");

        const auto audio = db.resolve_category("Audio", "Podcast");
        const auto legacy_audio = db.resolve_category("Music", "Podcast");
        CHECK(legacy_audio.taxonomy_id == audio.taxonomy_id);

        const auto installer = db.resolve_category("Software", "Installer Tools");
        const auto legacy_installer = db.resolve_category("Software", "Installer Builders");
        CHECK(legacy_installer.taxonomy_id == installer.taxonomy_id);
    }

    REQUIRE(sqlite3_open(db_path.c_str(), &raw_db) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(raw_db,
                               "SELECT COUNT(*) FROM category_taxonomy "
                               "WHERE normalized_category = 'music' OR normalized_subcategory = 'installer builders';",
                               -1,
                               &stmt,
                               nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    CHECK(sqlite3_column_int(stmt, 0) == 0);
    sqlite3_finalize(stmt);
    REQUIRE(sqlite3_close(raw_db) == SQLITE_OK);
}
