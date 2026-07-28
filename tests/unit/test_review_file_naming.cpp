#include <catch2/catch_test_macros.hpp>

#include "ReviewFileNaming.hpp"
#include "TestHelpers.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

TEST_CASE("ReviewFileNaming deduplicates duplicate image suggestions")
{
    TempDir temp_dir;

    CategorizedFile first;
    first.file_path = temp_dir.path().string();
    first.file_name = "a.png";
    first.type = FileType::File;
    first.suggested_name = "same_name.png";
    first.rename_only = true;

    CategorizedFile second = first;
    second.file_name = "b.png";

    std::vector<CategorizedFile> files{first, second};
    ReviewFileNaming::ensure_unique_image_suggested_names(
        files,
        temp_dir.path().string(),
        false);

    REQUIRE(files.size() == 2);
    CHECK(files[0].suggested_name == "same_name_1.png");
    CHECK(files[1].suggested_name == "same_name_2.png");
}

TEST_CASE("ReviewFileNaming avoids existing on-disk rename suggestions")
{
    TempDir temp_dir;
    std::ofstream(temp_dir.path() / "existing.png").put('x');

    std::unordered_set<std::string> used_names;
    std::unordered_map<std::string, int> next_index;

    const std::string unique = ReviewFileNaming::build_unique_suggested_name(
        "existing.png",
        temp_dir.path(),
        used_names,
        next_index,
        false);

    CHECK(unique == "existing_1.png");
}

TEST_CASE("ReviewFileNaming uses parenthetical suffixes for move collisions")
{
    TempDir temp_dir;
    std::ofstream(temp_dir.path() / "report.pdf").put('x');

    std::unordered_set<std::string> used_names;
    std::unordered_map<std::string, int> next_index;

    const std::string first = ReviewFileNaming::build_unique_move_name(
        "report.pdf",
        temp_dir.path(),
        used_names,
        next_index);
    used_names.insert(ReviewFileNaming::to_lower_copy_str(first));
    const std::string second = ReviewFileNaming::build_unique_move_name(
        "report.pdf",
        temp_dir.path(),
        used_names,
        next_index);

    CHECK(first == "report (1).pdf");
    CHECK(second == "report (2).pdf");
}
