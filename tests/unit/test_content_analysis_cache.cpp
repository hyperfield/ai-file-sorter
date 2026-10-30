#include "CacheMaintenanceService.hpp"
#include "ContentAnalysisCache.hpp"
#include "DatabaseManager.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>

namespace {
void write_source(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
    REQUIRE(output.good());
}
} // namespace

TEST_CASE("ContentAnalysisCache preserves full evidence across database reopen", "[content-cache]")
{
    TempDir config;
    TempDir source;
    const auto path = source.path() / "Document1.pdf";
    write_source(path, "Customer privacy retention policy");
    const auto key = ContentAnalysisCache::make_key(path, "document:test-model-v1");
    REQUIRE(key);
    const ContentAnalysisCache::Evidence evidence{
        "policy_changes.pdf", "Customer data retention and deletion requirements.\nApplies to hosted accounts."};
    {
        DatabaseManager database(config.path().string());
        REQUIRE(database.content_analysis_cache().store(*key, evidence));
    }
    DatabaseManager reopened(config.path().string());
    const auto cached = reopened.content_analysis_cache().find(*key);
    REQUIRE(cached);
    CHECK(cached->suggested_name == evidence.suggested_name);
    CHECK(cached->description == evidence.description);
    CHECK_FALSE(reopened.content_analysis_cache().find(
        *ContentAnalysisCache::make_key(path, "image:test-model-v1")));
    CHECK_FALSE(reopened.content_analysis_cache().find(
        *ContentAnalysisCache::make_key(path, "document:test-model-v2")));
}

TEST_CASE("ContentAnalysisCache shares native Unicode identity across lookup upsert and clear", "[storage-cache]")
{
    TempDir config;
    TempDir source;
    EnvVarGuard cache_file("CATEGORIZATION_CACHE_FILE", std::nullopt);
    DatabaseManager database(config.path().string());
    const auto composed_dir = source.path() / Utils::utf8_to_path("caf\xc3\xa9_%");
    const auto decomposed_dir = source.path() / Utils::utf8_to_path("cafe\xcc\x81_%");
    const auto composed_path = composed_dir / Utils::utf8_to_path("caf\xc3\xa9.txt");
    const auto decomposed_path = decomposed_dir / Utils::utf8_to_path("cafe\xcc\x81.txt");
    const auto nested_path = composed_dir / "nested" / "notes.txt";
    write_source(composed_path, "Same source");
    write_source(decomposed_path, "Same source");
    write_source(nested_path, "Nested source");
    const auto original = ContentAnalysisCache::make_key(composed_path, "document:v1");
    const auto equivalent = ContentAnalysisCache::make_key(decomposed_path, "document:v1");
    const auto nested = ContentAnalysisCache::make_key(nested_path, "document:v1");
    REQUIRE(original);
    REQUIRE(equivalent);
    REQUIRE(nested);
    auto cache = database.content_analysis_cache();
    REQUIRE(cache.store(*original, {"first.txt", "First evidence"}));
    REQUIRE(cache.store(*nested, {"notes.txt", "Nested evidence"}));
#if defined(__APPLE__)
    REQUIRE(cache.find(*equivalent));
#else
    CHECK_FALSE(cache.find(*equivalent));
#endif
    REQUIRE(cache.store(*equivalent, {"updated.txt", "Updated evidence"}));
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open((config.path() / "categorization_results.db").string().c_str(), &connection) == SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> raw(connection, sqlite3_close);
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(raw.get(), "SELECT COUNT(*) FROM content_analysis_cache;", -1, &statement, nullptr) == SQLITE_OK);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> count(statement, sqlite3_finalize);
    REQUIRE(sqlite3_step(count.get()) == SQLITE_ROW);
#if defined(__APPLE__)
    CHECK(sqlite3_column_int(count.get(), 0) == 2);
    CHECK(cache.find(*original)->description == "Updated evidence");
#else
    CHECK(sqlite3_column_int(count.get(), 0) == 3);
    CHECK(cache.find(*original)->description == "First evidence");
#endif
    count.reset();
    REQUIRE(database.clear_directory_categorizations(Utils::path_to_utf8(decomposed_dir), false));
    CHECK_FALSE(cache.find(*equivalent));
    REQUIRE(cache.find(*nested));
    REQUIRE(database.clear_directory_categorizations(Utils::path_to_utf8(decomposed_dir), true));
#if defined(__APPLE__)
    CHECK_FALSE(cache.find(*nested));
#else
    CHECK(cache.find(*nested));
#endif
}

TEST_CASE("ContentAnalysisCache rejects changed and incomplete evidence", "[content-cache]")
{
    TempDir config;
    TempDir source;
    const auto path = source.path() / "Document1.pdf";
    write_source(path, "First content");
    const auto before = ContentAnalysisCache::make_key(path, "document:v1");
    REQUIRE(before);
    DatabaseManager database(config.path().string());
    auto cache = database.content_analysis_cache();
    CHECK_FALSE(cache.find(*before));
    CHECK_FALSE(cache.store(*before, {"name_only.pdf", ""}));
    CHECK_FALSE(cache.store(*before, {"", "Description only"}));
    REQUIRE(cache.store(*before, {"first.pdf", "First summary"}));
    const auto modified = std::filesystem::last_write_time(path);
    write_source(path, "Other content");
    std::filesystem::last_write_time(path, modified);
    const auto after = ContentAnalysisCache::make_key(path, "document:v1");
    REQUIRE(after);
    CHECK(after->sha256 != before->sha256);
    CHECK_FALSE(cache.find(*after));
    CHECK_FALSE(cache.store(*before, {"stale.pdf", "Outdated summary"}));
    CHECK_FALSE(ContentAnalysisCache::make_key(source.path() / "missing.pdf", "document:v1"));
}

TEST_CASE("ContentAnalysisCache follows directory and global cache cleanup", "[content-cache]")
{
    TempDir config;
    TempDir source;
    DatabaseManager database(config.path().string());
    const auto root = source.path() / "Client_%";
    const auto sibling = source.path() / "Client_AB";
    const auto direct = root / "direct.pdf";
    const auto nested = root / "Projects" / "nested.pdf";
    const auto other = sibling / "other.pdf";
    for (const auto& path : {direct, nested, other}) {
        write_source(path, "Content");
        REQUIRE(database.content_analysis_cache().store(
            *ContentAnalysisCache::make_key(path, "document:v1"), {"content.pdf", "Complete content summary"}));
    }
    CacheMaintenanceService maintenance(config.path().string());
    CHECK(maintenance.target_info(CacheMaintenanceTarget::Categorization).size_bytes > 0);
    REQUIRE(database.clear_directory_categorizations(root.string(), false));
    CHECK_FALSE(database.content_analysis_cache().find(*ContentAnalysisCache::make_key(direct, "document:v1")));
    REQUIRE(database.content_analysis_cache().find(*ContentAnalysisCache::make_key(nested, "document:v1")));
    REQUIRE(database.clear_directory_categorizations(root.string(), true));
    CHECK_FALSE(database.content_analysis_cache().find(*ContentAnalysisCache::make_key(nested, "document:v1")));
    REQUIRE(database.content_analysis_cache().find(*ContentAnalysisCache::make_key(other, "document:v1")));
    REQUIRE(database.clear_all_categorizations());
    CHECK_FALSE(database.content_analysis_cache().find(*ContentAnalysisCache::make_key(other, "document:v1")));
    CHECK(maintenance.target_info(CacheMaintenanceTarget::Categorization).size_bytes == 0);
}
