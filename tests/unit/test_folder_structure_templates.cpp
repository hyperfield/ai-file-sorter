#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "FolderStructureTemplates.hpp"
#include "TestHelpers.hpp"

namespace {

bool has_template(FolderStructureTemplates::Id id) {
    return FolderStructureTemplates::find(id) != nullptr;
}

}  // namespace

TEST_CASE("FolderStructureTemplates exposes requested starter structures") {
    using FolderStructureTemplates::Id;

    CHECK(has_template(Id::JohnnyDecimal));
    CHECK(has_template(Id::Para));
    CHECK(has_template(Id::Gtd));
    CHECK(has_template(Id::Chronological));
    CHECK(has_template(Id::ClientProjectDeliverable));
    CHECK(has_template(Id::DepartmentFunction));
    CHECK(has_template(Id::MediaType));
    CHECK(has_template(Id::StatusLifecycle));
    CHECK(has_template(Id::KnowledgeBase));
    CHECK(has_template(Id::NumberedPrefix));

    for (const auto& descriptor : FolderStructureTemplates::all()) {
        CHECK_FALSE(descriptor.name.empty());
        CHECK_FALSE(descriptor.description.empty());
        if (descriptor.available) {
            CHECK_FALSE(descriptor.relative_directories.empty());
        }
    }

    REQUIRE(FolderStructureTemplates::find(Id::Chronological) != nullptr);
    REQUIRE(FolderStructureTemplates::find(Id::ClientProjectDeliverable) != nullptr);
    CHECK_FALSE(FolderStructureTemplates::find(Id::Chronological)->available);
    CHECK_FALSE(FolderStructureTemplates::find(Id::ClientProjectDeliverable)->available);
}

TEST_CASE("FolderStructureTemplates creates starter structure without overwriting existing folders") {
    using FolderStructureTemplates::Id;

    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "Projects"));

    const auto result = FolderStructureTemplates::create(temp_dir.path(), Id::Para);

    REQUIRE(result.success);
    CHECK(std::filesystem::is_directory(temp_dir.path() / "Projects"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "Projects" / "Active"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "Areas"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "Resources"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "Archives"));
    CHECK_FALSE(result.created_directories.empty());
    CHECK_FALSE(result.existing_directories.empty());

    const auto second_result = FolderStructureTemplates::create(temp_dir.path(), Id::Para);
    REQUIRE(second_result.success);
    CHECK(second_result.created_directories.empty());
    CHECK(second_result.existing_directories.size() >= result.created_directories.size());
}

TEST_CASE("FolderStructureTemplates creates Johnny Decimal starter folders") {
    using FolderStructureTemplates::Id;

    TempDir temp_dir;
    const auto result = FolderStructureTemplates::create(temp_dir.path(), Id::JohnnyDecimal);

    REQUIRE(result.success);
    CHECK(std::filesystem::is_directory(temp_dir.path() / "10-19 Admin" / "11 Finance"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "30-39 Personal" / "32 Health"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "40-49 Media" / "41 Photos"));
    CHECK(std::filesystem::is_directory(temp_dir.path() / "90-99 Archive" / "91 Completed"));
}

TEST_CASE("FolderStructureTemplates rejects unavailable starter structures") {
    using FolderStructureTemplates::Id;

    TempDir temp_dir;
    const auto result = FolderStructureTemplates::create(temp_dir.path(), Id::Chronological);

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error.empty());
    CHECK(result.created_directories.empty());
    CHECK(std::filesystem::is_empty(temp_dir.path()));
}

TEST_CASE("FolderStructureTemplates rejects relative destination paths") {
    using FolderStructureTemplates::Id;

    const auto result = FolderStructureTemplates::create("relative-folder", Id::MediaType);

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error.empty());
    CHECK(result.created_directories.empty());
}
