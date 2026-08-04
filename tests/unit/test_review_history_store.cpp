#include <catch2/catch_test_macros.hpp>

#include "ReviewHistoryStore.hpp"
#include "TestHelpers.hpp"

TEST_CASE("ReviewHistoryStore persists, searches, and marks history entries undone")
{
    TempDir config_dir;
    ReviewHistoryStore store(config_dir.path().string());
    REQUIRE(store.is_open());

    ReviewHistoryStore::Entry entry;
    entry.provider_id = "local_fs";
    entry.operation = ReviewHistoryStore::Operation::RenameAndCategorize;
    entry.source_path = "/inbox/scan001.jpg";
    entry.destination_path = "/photos/Family/lake_picnic.jpg";
    entry.original_file_name = "scan001.jpg";
    entry.final_file_name = "lake_picnic.jpg";
    entry.category = "Photos";
    entry.subcategory = "Family";
    entry.file_description = "Sunny picnic near the lake.";
    entry.size_bytes = 12;
    entry.mtime = 42;
    entry.stable_identity = "identity";
    entry.revision_token = "12:42";

    std::string error;
    const auto id = store.record_entry(entry, &error);
    REQUIRE(id.has_value());
    CHECK(error.empty());

    const auto by_filename = store.search_entries("lake_picnic");
    REQUIRE(by_filename.size() == 1);
    CHECK(by_filename.front().id == *id);
    CHECK(by_filename.front().operation == ReviewHistoryStore::Operation::RenameAndCategorize);
    CHECK(by_filename.front().file_description == entry.file_description);

    const auto by_category = store.search_entries("family");
    REQUIRE(by_category.size() == 1);
    CHECK(by_category.front().id == *id);

    const auto by_description = store.search_entries("picnic");
    REQUIRE(by_description.size() == 1);
    CHECK(by_description.front().id == *id);

    REQUIRE(store.mark_undone(*id, &error));
    const auto reloaded_entry = store.entry_by_id(*id);
    REQUIRE(reloaded_entry.has_value());
    CHECK(reloaded_entry->undone);
    CHECK_FALSE(reloaded_entry->undone_at_utc.empty());

    ReviewHistoryStore reloaded(config_dir.path().string());
    REQUIRE(reloaded.is_open());
    const auto all_entries = reloaded.entries();
    REQUIRE(all_entries.size() == 1);
    CHECK(all_entries.front().id == *id);
    CHECK(all_entries.front().undone);
}
