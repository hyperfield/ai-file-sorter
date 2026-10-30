#include "CacheMaintenanceService.hpp"
#include "ContentAnalysisCache.hpp"
#include "DatabaseManager.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

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
