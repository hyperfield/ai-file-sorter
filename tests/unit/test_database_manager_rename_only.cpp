#include <catch2/catch_test_macros.hpp>

#include "DatabaseManager.hpp"
#include "DatabaseTaxonomyNormalizer.hpp"
#include "CloudCompatibilityProvider.hpp"
#include "ExternalProcessStorageProvider.hpp"
#include "LocalFsProvider.hpp"
#include "TestHelpers.hpp"

#include <QString>
#include <sqlite3.h>

#include <memory>
#include <functional>

namespace {
class NormalizingStorageProvider : public LocalFsProvider {
public:
    std::string comparison_key(const std::string& path) const override {
        return QString::fromStdString(path).normalized(QString::NormalizationForm_C).toStdString();
    }
};

class CountingStorageProvider : public NormalizingStorageProvider {
public:
    std::string comparison_key(const std::string& path) const override {
        ++comparisons;
        return NormalizingStorageProvider::comparison_key(path);
    }
    mutable size_t comparisons{0};
};

using RawDatabase = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using RawStatement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

void execute_sql(sqlite3* db, const std::string& sql) {
    const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    INFO(sql);
    INFO(sqlite3_errmsg(db));
    REQUIRE(result == SQLITE_OK);
}

RawStatement query(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    const int result = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    RawStatement statement(raw, sqlite3_finalize);
    INFO(sql);
    REQUIRE(result == SQLITE_OK);
    return statement;
}

sqlite3_int64 query_integer(sqlite3* db, const char* sql) {
    auto statement = query(db, sql);
    REQUIRE(sqlite3_step(statement.get()) == SQLITE_ROW);
    return sqlite3_column_int64(statement.get(), 0);
}

std::string query_text(sqlite3* db, const char* sql) {
    auto statement = query(db, sql);
    REQUIRE(sqlite3_step(statement.get()) == SQLITE_ROW);
    return reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0));
}

RawDatabase legacy_unicode_database(const TempDir& directory) {
    {
        DatabaseManager initialize(directory.path().string());
    }
    sqlite3* raw = nullptr;
    const int result = sqlite3_open((directory.path() / "categorization_results.db").string().c_str(), &raw);
    RawDatabase db(raw, sqlite3_close);
    REQUIRE(result == SQLITE_OK);
    execute_sql(db.get(), R"sql(
        INSERT INTO category_taxonomy
            (id, canonical_category, canonical_subcategory, normalized_category, normalized_subcategory, frequency)
        VALUES
            (10, 'Café', 'Økonomi', CAST(X'636166E3' AS TEXT), 'broken-economy', 2),
            (20, 'CAFÉ', 'ØKONOMI', 'legacy-cafe', 'legacy-economy', 3),
            (30, '会议', '资料', '会计', '资料', 7),
            (40, '会计', '资料', '会议', '资料', 11);
        INSERT INTO file_categorization
            (file_name, file_type, dir_path, category, subcategory, taxonomy_id)
        VALUES ('coffee.txt', 'F', '/sample', 'Café', 'Økonomi', 20),
               ('meetings.txt', 'F', '/sample', '会议', '资料', 30),
               ('accounts.txt', 'F', '/sample', '会计', '资料', 40);
        INSERT INTO category_alias (alias_category_norm, alias_subcategory_norm, taxonomy_id)
        VALUES ('coffee', 'economy', 20),
               ('meeting', 'notes', 30),
               (CAST(X'636166E3' AS TEXT), 'broken-economy', 20);
        INSERT INTO category_translation
            (taxonomy_id, language, category, subcategory, normalized_category, normalized_subcategory, updated_at)
        VALUES (10, 'french', 'Café', 'Économie', 'caf', 'conomie', '2025-01-01 00:00:00'),
               (20, 'french', 'CAFÉ', 'ÉCONOMIE', 'broken-cafe', 'broken-economy', '2026-01-01 00:00:00'),
               (20, 'danish', 'Kaffe', 'Økonomi', 'kaffe', 'broken-economy', '2025-06-01 00:00:00');
    )sql");
    return db;
}
} // namespace

TEST_CASE("Taxonomy normalization preserves Unicode letters numbers and attached marks") {
    using DatabaseTaxonomyNormalizer::normalize_label;
    CHECK(normalize_label("  CAFÉ\xc2\xa0 Reports  ") == "café reports");
    CHECK(normalize_label("Cafe\xcc\x81") == normalize_label("Café"));
    CHECK(normalize_label("ØKONOMI") == "økonomi");
    CHECK(normalize_label("会议 资料") == "会议 资料");
    CHECK(normalize_label("अनुसंधान") == "अनुसंधान");
    CHECK(normalize_label("कला") != normalize_label("कली"));
    CHECK(normalize_label("\xf0\x90\x90\x80") == "\xf0\x90\x90\xa8");
    CHECK(normalize_label("Images: Photo_Files") == "images photofiles");
    CHECK(normalize_label("Cloud ☁️") == "cloud");
}

TEST_CASE("Taxonomy fuzzy matching compares Unicode code points rather than UTF-8 bytes") {
    CHECK(DatabaseTaxonomyNormalizer::string_similarity("会议", "会计") == 0.5);
    CHECK(DatabaseTaxonomyNormalizer::string_similarity("ab", "ac") == 0.5);
    CHECK(DatabaseTaxonomyNormalizer::string_similarity("\xf0\x90\x90\xa8", "\xf0\x90\x90\xa9") == 0.0);
}

TEST_CASE("DatabaseManager matches canonical Unicode forms without merging distinct short labels") {
    TempDir root;
    DatabaseManager db(root.path().string());
    const auto cafe = db.resolve_category("Café", "Økonomi");
    REQUIRE(cafe.taxonomy_id > 0);
    CHECK(db.resolve_category("CAFE\xcc\x81", "ØKONOMI").taxonomy_id == cafe.taxonomy_id);
    const auto meetings = db.resolve_category("会议", "资料");
    const auto accounting = db.resolve_category("会计", "资料");
    REQUIRE(meetings.taxonomy_id > 0);
    REQUIRE(accounting.taxonomy_id > 0);
    CHECK(meetings.taxonomy_id != accounting.taxonomy_id);
}

TEST_CASE("DatabaseManager reuses cached Unicode paths for native filesystems", "[storage-cache]") {
    TempDir root;
    DatabaseManager db(root.path().string());
    const std::string composed_dir = "/sample/caf\xc3\xa9_%";
    const std::string decomposed_dir = "/sample/cafe\xcc\x81_%";
    const std::string composed_name = "caf\xc3\xa9.txt";
    const std::string decomposed_name = "cafe\xcc\x81.txt";
    const DatabaseManager::ResolvedCategory labels{0, "Documents", "Notes"};
    REQUIRE(db.insert_or_update_file_with_categorization(composed_name, "F", composed_dir, labels, true));
    REQUIRE(db.insert_or_update_file_with_categorization("nested.txt", "F", composed_dir + "/nested", labels, false));
    REQUIRE(db.insert_or_update_file_with_categorization("sibling.txt", "F", composed_dir + "-other", labels, false));
    REQUIRE(db.insert_or_update_file_with_categorization("wildcard.txt", "F", "/sample/café_AX/nested", labels, false));
#if defined(__APPLE__)
    REQUIRE(db.get_categorized_files(decomposed_dir).size() == 1);
    CHECK(db.get_categorized_files_recursive(decomposed_dir).size() == 2);
    const auto cached = db.get_categorized_file(decomposed_dir, decomposed_name, FileType::File);
    REQUIRE(cached);
    CHECK(cached->file_name == composed_name);
    CHECK(cached->file_path == composed_dir);
    CHECK(db.get_categorization_from_db(decomposed_dir, decomposed_name, FileType::File).size() == 2);
    CHECK(db.get_dir_contents_from_db(decomposed_dir) == std::vector<std::string>{composed_name});
    CHECK(db.is_file_already_categorized(decomposed_name));
    CHECK(db.get_directory_categorization_style(decomposed_dir) == std::optional<bool>{true});
    CHECK(db.has_categorization_style_conflict(decomposed_dir, false));
    CHECK(db.has_categorization_style_conflict(decomposed_dir, true, true));
    REQUIRE(db.remove_file_categorization(decomposed_dir, decomposed_name, FileType::File));
    CHECK(db.get_categorized_files(composed_dir).empty());
    REQUIRE(db.clear_directory_categorizations(decomposed_dir, true));
    CHECK(db.get_categorized_files_recursive(composed_dir).empty());
    CHECK(db.get_categorized_files(composed_dir + "-other").size() == 1);
    CHECK(db.get_categorized_files_recursive("/sample/café_AX").size() == 1);
#else
    CHECK(db.get_categorized_files(decomposed_dir).empty());
    CHECK_FALSE(db.get_categorized_file(composed_dir, decomposed_name, FileType::File));
    CHECK(db.get_categorized_files_recursive(composed_dir).size() == 2);
#endif
}

TEST_CASE("DatabaseManager upserts equivalent native filenames without duplicate cache rows", "[storage-cache]") {
    TempDir root;
    DatabaseManager db(root.path().string());
    const std::string composed = "caf\xc3\xa9";
    const std::string decomposed = "cafe\xcc\x81";
    const DatabaseManager::ResolvedCategory first{0, "Documents", "Notes"};
    const DatabaseManager::ResolvedCategory updated{0, "Documents", "Reports"};
    REQUIRE(db.insert_or_update_file_with_categorization(composed + ".txt", "F", "/sample/" + composed,
                                                       first, false, "renamed.txt", false, true));
    REQUIRE(db.insert_or_update_file_with_categorization(decomposed + ".txt", "F", "/sample/" + decomposed,
                                                       updated, true));
    const auto cached = db.get_categorized_file("/sample/" + decomposed, decomposed + ".txt", FileType::File);
    REQUIRE(cached);
    CHECK(cached->subcategory == "Reports");
    CHECK(cached->file_name == decomposed + ".txt");
#if defined(__APPLE__)
    CHECK(cached->rename_applied);
    CHECK(db.get_categorized_files_recursive("/sample").size() == 1);
#else
    CHECK_FALSE(cached->rename_applied);
    CHECK(db.get_categorized_files_recursive("/sample").size() == 2);
#endif
}

TEST_CASE("DatabaseManager follows provider comparison rules without rewriting legacy names", "[storage-cache]") {
    TempDir root;
    DatabaseManager db(root.path().string());
    ExternalProcessStorageProvider remote(StoragePluginManifest{}, "remote", "remote", false);
    CloudCompatibilityProvider cloud("custom_sync", "Sync", {"sync"});
    NormalizingStorageProvider unicode_provider;
    const std::string composed = "caf\xc3\xa9.txt";
    const std::string decomposed = "cafe\xcc\x81.txt";
    db.set_storage_provider(remote);
    REQUIRE(db.insert_or_update_file_with_categorization(composed, "F", "/sample", {0, "Documents", "Notes"},
                                                       false, "done.txt", false, true));
    CHECK_FALSE(db.get_categorized_file("/sample", decomposed, FileType::File));
    REQUIRE(db.insert_or_update_file_with_categorization(decomposed, "F", "/sample", {0, "Documents", "Reports"}, false));
    CHECK(db.get_categorized_files("/sample").size() == 2);
    db.set_storage_provider(cloud);
#if defined(__APPLE__)
    CHECK(db.get_categorized_files("/sample").size() == 1);
#else
    CHECK(db.get_categorized_files("/sample").size() == 2);
#endif
    db.set_storage_provider(unicode_provider);
    auto entries = db.get_categorized_files("/sample");
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().file_name == decomposed);
    CHECK(entries.front().subcategory == "Reports");
    CHECK(entries.front().rename_applied);
    const auto equivalent = db.get_categorized_file("/sample", composed, FileType::File);
    REQUIRE(equivalent);
    CHECK(equivalent->file_name == decomposed);
    CHECK(equivalent->rename_applied);
    db.set_storage_provider(remote);
    CHECK(db.get_categorized_files("/sample").size() == 2);
    // Merely switching provider rules must not merge or rewrite persistent rows.
    CHECK(db.get_categorized_file("/sample", composed, FileType::File)->subcategory == "Notes");
}

TEST_CASE("DatabaseManager cleans empty legacy duplicates without removing valid Unicode entries", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    execute_sql(raw.get(), R"sql(
        INSERT INTO file_categorization(file_name, file_type, dir_path, category, subcategory)
        VALUES ('café.txt', 'F', '/sample/café', 'Documents', 'Notes'),
               ('café.txt', 'F', '/sample/café', '', '');
    )sql");
    NormalizingStorageProvider provider;
    db.set_storage_provider(provider);
    REQUIRE(db.remove_empty_categorizations("/sample/café").size() == 1);
    const auto entries = db.get_categorized_files("/sample/cafe\xcc\x81");
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().subcategory == "Notes");
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization;") == 1);
}

TEST_CASE("DatabaseManager matches and updates Unicode folder routing identities", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    NormalizingStorageProvider provider;
    db.set_storage_provider(provider);
    DatabaseManager::FolderTreeRoutingRecord route;
    route.destination_root = "/target/café";
    route.tree_fingerprint = "tree:v1";
    route.semantic_category = "Documents";
    route.semantic_subcategory = "Notes";
    route.target_folder_relative_path = "Notes";
    REQUIRE(db.insert_or_update_folder_tree_routing("café.txt", "F", "/source/café", route));
    auto lookup = [&](const std::string& fingerprint) {
        return db.get_folder_tree_routing("cafe\xcc\x81.txt", FileType::File, "/source/cafe\xcc\x81",
            "/target/cafe\xcc\x81", fingerprint, false, "Documents", "Notes");
    };
    REQUIRE(lookup("tree:v1"));
    route.destination_root = "/target/cafe\xcc\x81";
    route.target_folder_relative_path = "Updated Notes";
    REQUIRE(db.insert_or_update_folder_tree_routing("cafe\xcc\x81.txt", "F", "/source/cafe\xcc\x81", route));
    CHECK(lookup("tree:v1")->target_folder_relative_path == "Updated Notes");
    route.tree_fingerprint = "tree:v2";
    REQUIRE(db.insert_or_update_folder_tree_routing("café.txt", "F", "/source/café", route));
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM folder_tree_routing;") == 2);
    REQUIRE(db.remove_file_categorization("/source/café", "café.txt", FileType::File));
    CHECK_FALSE(lookup("tree:v1"));
    CHECK_FALSE(lookup("tree:v2"));
}

TEST_CASE("DatabaseManager rolls back Unicode upserts if duplicate cleanup fails", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    NormalizingStorageProvider provider;
    db.set_storage_provider(provider);
    REQUIRE(db.insert_or_update_file_with_categorization("café.txt", "F", "/sample", {0, "Documents", "Notes"}, false));
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    execute_sql(raw.get(), "CREATE TRIGGER reject_cache_cleanup BEFORE DELETE ON file_categorization "
                          "BEGIN SELECT RAISE(ABORT, 'test cache cleanup failure'); END;");
    CHECK_FALSE(db.insert_or_update_file_with_categorization("cafe\xcc\x81.txt", "F", "/sample",
                                                          {0, "Documents", "Reports"}, false));
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization;") == 1);
    const auto cached = db.get_categorized_file("/sample", "café.txt", FileType::File);
    REQUIRE(cached);
    CHECK(cached->subcategory == "Notes");
}

TEST_CASE("DatabaseManager avoids per-row comparison work for indexed cache operations", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    execute_sql(raw.get(), R"sql(
        WITH RECURSIVE entries(i) AS (VALUES(1) UNION ALL SELECT i + 1 FROM entries WHERE i < 10000)
        INSERT INTO file_categorization(file_name, file_type, dir_path, category, subcategory)
        SELECT 'file-' || i || '.txt', 'F', '/sample', 'Documents', 'Notes' FROM entries;
    )sql");
    CountingStorageProvider provider;
    db.set_storage_provider(provider);
    DatabaseManager::FolderTreeRoutingRecord route;
    route.destination_root = "/target";
    route.tree_fingerprint = "tree:v1";
    route.semantic_category = "Documents";
    route.semantic_subcategory = "Notes";
    route.target_folder_relative_path = "Notes";
    REQUIRE(db.insert_or_update_folder_tree_routing("file-9000.txt", "F", "/sample", route));
    execute_sql(raw.get(), R"sql(
        INSERT INTO folder_tree_routing(file_name,file_type,dir_path,destination_root,tree_fingerprint,
            semantic_category,semantic_subcategory,target_folder_relative_path)
        SELECT file_name,file_type,dir_path,'/target',
            (SELECT tree_fingerprint FROM folder_tree_routing LIMIT 1),'Documents','Notes','Notes'
        FROM file_categorization WHERE file_name != 'file-9000.txt';
        INSERT INTO content_analysis_cache
        SELECT '/sample/' || file_name,dir_path,'document:v1','hash',file_name,'Evidence'
        FROM file_categorization;
    )sql");
    REQUIRE(db.get_categorized_file("/sample", "file-9000.txt", FileType::File));
    provider.comparisons = 0;
    for (int index = 9000; index < 9100; ++index) {
        const auto name = "file-" + std::to_string(index) + ".txt";
        REQUIRE(db.get_categorized_file("/sample", name, FileType::File));
        REQUIRE(db.insert_or_update_file_with_categorization(name, "F", "/sample", {0, "Documents", "Notes"}, false));
        REQUIRE(db.get_folder_tree_routing(name, FileType::File, "/sample", "/target", "tree:v1",
                                          false, "Documents", "Notes"));
        REQUIRE(db.insert_or_update_folder_tree_routing(name, "F", "/sample", route));
        REQUIRE(db.content_analysis_cache().find({"/sample/" + name, "document:v1", "hash"}));
    }
    // No wall-clock threshold: repeated operations must not normalize the entire 10,000-row cache.
    CHECK(provider.comparisons < 5000);
}

TEST_CASE("DatabaseManager rolls back cache clearing when a dependent deletion fails", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    const auto labels = db.resolve_category("Documents", "Notes");
    REQUIRE(db.insert_or_update_file_with_categorization("notes.txt", "F", "/sample", labels, false));
    DatabaseManager::FolderTreeRoutingRecord route;
    route.destination_root = "/target";
    route.tree_fingerprint = "tree:v1";
    route.semantic_category = "Documents";
    route.semantic_subcategory = "Notes";
    route.target_folder_relative_path = "Notes";
    REQUIRE(db.insert_or_update_folder_tree_routing("notes.txt", "F", "/sample", route));
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    execute_sql(raw.get(), "INSERT INTO content_analysis_cache VALUES "
        "('/sample/notes.txt', '/sample', 'document:v1', 'hash', 'notes.txt', 'Evidence');");
    std::string rejection_table = "folder_tree_routing";
    std::function<bool()> clear;
    bool single_file = false;
    bool clear_taxonomy = false;
    SECTION("Single-file routing failure") {
        single_file = true;
        clear = [&] { return db.remove_file_categorization("/sample", "notes.txt", FileType::File); };
    }
    SECTION("Nonrecursive directory routing failure") {
        clear = [&] { return db.clear_directory_categorizations("/sample"); };
    }
    SECTION("Recursive directory evidence failure") {
        rejection_table = "content_analysis_cache";
        clear = [&] { return db.clear_directory_categorizations("/sample", true); };
    }
    SECTION("Global routing failure") {
        clear = [&] { return db.clear_all_categorizations(); };
    }
    SECTION("Global taxonomy failure") {
        rejection_table = "category_taxonomy";
        clear_taxonomy = true;
        clear = [&] { return db.clear_all_categorizations(true); };
    }
    execute_sql(raw.get(), "CREATE TRIGGER reject_clear BEFORE DELETE ON " + rejection_table +
        " BEGIN SELECT RAISE(ABORT, 'test dependent deletion failure'); END;");
    CHECK_FALSE(clear());
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization;") == 1);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM folder_tree_routing;") == 1);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM content_analysis_cache;") == 1);
    CHECK(db.resolve_category("Documents", "Notes").taxonomy_id == labels.taxonomy_id);
    execute_sql(raw.get(), "DROP TRIGGER reject_clear;");
    REQUIRE(clear());
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization;") == 0);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM folder_tree_routing;") == 0);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM content_analysis_cache;") == (single_file ? 1 : 0));
    CHECK(db.get_taxonomy_snapshot(0).empty() == clear_taxonomy);
}

TEST_CASE("DatabaseManager sees cache writes from other database connections", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    NormalizingStorageProvider provider;
    db.set_storage_provider(provider);
    CHECK(db.get_categorized_files("/sample").empty());
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    execute_sql(raw.get(), "INSERT INTO file_categorization(file_name,file_type,dir_path,category,subcategory) "
        "VALUES('café.txt','F','/sample','Documents','Notes');");
    REQUIRE(db.get_categorized_file("/sample", "cafe\xcc\x81.txt", FileType::File));
    execute_sql(raw.get(), "UPDATE file_categorization SET file_name='résumé.txt',dir_path='/changed';");
    CHECK_FALSE(db.get_categorized_file("/sample", "café.txt", FileType::File));
    REQUIRE(db.get_categorized_file("/changed", "re\xcc\x81sume\xcc\x81.txt", FileType::File));
    execute_sql(raw.get(), "DELETE FROM file_categorization;");
    CHECK(db.get_categorized_files("/changed").empty());
}

TEST_CASE("DatabaseManager reuses lookups without retaining bindings or read locks", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    REQUIRE(db.insert_or_update_file_with_categorization("entry", "F", "/sample", {0, "Documents", "Notes"}, false));
    REQUIRE(db.insert_or_update_file_with_categorization("entry", "D", "/sample", {0, "Documents", "Folders"}, false));
    const auto file = db.get_categorized_file("/sample", "entry", FileType::File);
    const auto directory = db.get_categorized_file("/sample", "entry", FileType::Directory);
    REQUIRE(file);
    REQUIRE(directory);
    CHECK(file->subcategory == "Notes");
    CHECK(directory->subcategory == "Folders");
    CHECK_FALSE(db.get_categorized_file("/other", "entry", FileType::File));
    CHECK_FALSE(db.get_categorized_file("/sample", "absent", FileType::File));

    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    // A retained read cursor would prevent this commit on a separate connection.
    execute_sql(raw.get(), "BEGIN IMMEDIATE; UPDATE file_categorization SET subcategory='Updated' "
                          "WHERE file_type='F'; COMMIT;");
    const auto updated = db.get_categorized_file("/sample", "entry", FileType::File);
    REQUIRE(updated);
    CHECK(updated->subcategory == "Updated");
    execute_sql(raw.get(), "ALTER TABLE file_categorization ADD COLUMN test_schema_change TEXT;");
    const auto after_schema_change = db.get_categorized_file("/sample", "entry", FileType::Directory);
    REQUIRE(after_schema_change);
    CHECK(after_schema_change->subcategory == "Folders");
}

TEST_CASE("DatabaseManager rolls back empty-row pruning if a later deletion fails", "[storage-cache]") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager db(root.path().string());
    REQUIRE(db.insert_or_update_file_with_categorization("first.txt", "F", "/sample", {0, "", ""}, false));
    REQUIRE(db.insert_or_update_file_with_categorization("second.txt", "F", "/sample", {0, "", ""}, false));
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((root.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    RawDatabase raw(connection, sqlite3_close);
    execute_sql(raw.get(), "CREATE TRIGGER reject_last_empty_delete BEFORE DELETE ON file_categorization "
        "WHEN (SELECT COUNT(*) FROM file_categorization) = 1 "
        "BEGIN SELECT RAISE(ABORT, 'test later deletion failure'); END;");
    CHECK(db.remove_empty_categorizations("/sample").empty());
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization;") == 2);
    execute_sql(raw.get(), "DROP TRIGGER reject_last_empty_delete;");
    CHECK(db.remove_empty_categorizations("/sample").size() == 2);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization;") == 0);
}

TEST_CASE("DatabaseManager repairs stored Unicode taxonomy keys and preserves references on reopen") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    auto raw = legacy_unicode_database(root);
    {
        DatabaseManager db(root.path().string());
        CHECK(db.resolve_category("CAFE\xcc\x81", "ØKONOMI").taxonomy_id == 10);
        CHECK(db.resolve_category("coffee", "economy").taxonomy_id == 10);
        CHECK(db.resolve_category("会议", "资料").taxonomy_id == 30);
        CHECK(db.resolve_category("会计", "资料").taxonomy_id == 40);
        const auto cached = db.get_categorized_file("/sample", "coffee.txt", FileType::File);
        REQUIRE(cached.has_value());
        CHECK(cached->taxonomy_id == 10);
        CHECK(cached->category == "Café");
        CHECK(cached->subcategory == "Økonomi");
        const auto french = db.get_category_translation(10, CategoryLanguage::French);
        REQUIRE(french.has_value());
        CHECK(french->category == "CAFÉ");
        CHECK(french->subcategory == "ÉCONOMIE");
        const auto danish = db.get_category_translation(10, CategoryLanguage::Danish);
        REQUIRE(danish.has_value());
        CHECK(danish->subcategory == "Økonomi");
        CHECK(db.resolve_category_for_language("cafe\xcc\x81", "économie", CategoryLanguage::French).taxonomy_id == 10);
    }
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM category_taxonomy;") == 3);
    CHECK(query_integer(raw.get(), "SELECT frequency FROM category_taxonomy WHERE id = 10;") == 5);
    CHECK(query_integer(raw.get(), "SELECT frequency FROM category_taxonomy WHERE id = 30;") == 7);
    CHECK(query_integer(raw.get(), "SELECT frequency FROM category_taxonomy WHERE id = 40;") == 11);
    CHECK(query_text(raw.get(), "SELECT normalized_category FROM category_taxonomy WHERE id = 10;") == "café");
    CHECK(query_text(raw.get(), "SELECT normalized_subcategory FROM category_taxonomy WHERE id = 10;") == "økonomi");
    CHECK(query_integer(raw.get(), "SELECT taxonomy_id FROM category_alias WHERE alias_category_norm = 'coffee';") == 10);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM category_alias WHERE alias_category_norm = CAST(X'636166E3' AS TEXT);") == 0);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM file_categorization WHERE taxonomy_id = 20;") == 0);
    CHECK(query_text(raw.get(), "SELECT updated_at FROM category_translation WHERE taxonomy_id = 10 AND language = 'french';") == "2026-01-01 00:00:00");
    auto integrity = query(raw.get(), "PRAGMA foreign_key_check;");
    CHECK(sqlite3_step(integrity.get()) == SQLITE_DONE);
    integrity.reset();

    execute_sql(raw.get(), "CREATE TRIGGER reject_repeat_rekey BEFORE UPDATE OF normalized_category ON category_taxonomy "
                          "BEGIN SELECT RAISE(ABORT, 'unexpected repeated migration'); END;");
    {
        DatabaseManager reopened(root.path().string());
        CHECK(reopened.resolve_category("Café", "Økonomi").taxonomy_id == 10);
    }
    CHECK(query_integer(raw.get(), "SELECT frequency FROM category_taxonomy WHERE id = 10;") == 5);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM category_translation;") == 2);
}

TEST_CASE("DatabaseManager rolls back Unicode key migration completely when a write fails") {
    TempDir root;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    auto raw = legacy_unicode_database(root);
    execute_sql(raw.get(), "CREATE TRIGGER reject_taxonomy_merge BEFORE DELETE ON category_taxonomy WHEN OLD.id = 20 "
                          "BEGIN SELECT RAISE(ABORT, 'test migration failure'); END;");
    {
        DatabaseManager failed(root.path().string());
        CHECK(failed.resolve_category("Café", "Økonomi").taxonomy_id == -1);
    }
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM category_taxonomy;") == 4);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM category_alias;") == 3);
    CHECK(query_integer(raw.get(), "SELECT COUNT(*) FROM category_translation;") == 3);
    CHECK(query_integer(raw.get(), "SELECT taxonomy_id FROM file_categorization WHERE file_name = 'coffee.txt';") == 20);
    CHECK(query_text(raw.get(), "SELECT hex(normalized_category) FROM category_taxonomy WHERE id = 10;") == "636166E3");
    CHECK(query_integer(raw.get(), "SELECT frequency FROM category_taxonomy WHERE id = 10;") == 2);
    execute_sql(raw.get(), "DROP TRIGGER reject_taxonomy_merge;");
    {
        DatabaseManager recovered(root.path().string());
        CHECK(recovered.resolve_category("Café", "Økonomi").taxonomy_id == 10);
    }
}

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
