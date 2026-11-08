#include <catch2/catch_test_macros.hpp>

#include "ReviewFileNaming.hpp"
#include "TestHelpers.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

TEST_CASE("ReviewFileNaming deduplicates duplicate regular-file suggestions")
{
    TempDir temp_dir;

    CategorizedFile first;
    first.file_path = temp_dir.path().string();
    first.file_name = "a.mp4";
    first.type = FileType::File;
    first.category = "Media files";
    first.suggested_name = "2025_videohandle.mp4";

    CategorizedFile second = first;
    second.file_name = "b.mp4";

    std::vector<CategorizedFile> files{first, second};
    ReviewFileNaming::ensure_unique_suggested_names(
        files,
        temp_dir.path().string(),
        false);

    REQUIRE(files.size() == 2);
    CHECK(files[0].suggested_name == "2025_videohandle_1.mp4");
    CHECK(files[1].suggested_name == "2025_videohandle_2.mp4");
}

TEST_CASE("ReviewFileNaming deduplicates same-folder renames against the source directory")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "Media files"));
    std::ofstream(temp_dir.path() / "Media files" / "2025_videohandle.mp4").put('x');

    CategorizedFile file;
    file.file_path = temp_dir.path().string();
    file.file_name = "clip.mp4";
    file.type = FileType::File;
    file.category = "Media files";
    file.suggested_name = "2025_videohandle.mp4";

    std::vector<CategorizedFile> files{file};
    ReviewFileNaming::ensure_unique_suggested_names(
        files,
        temp_dir.path().string(),
        false,
        false);

    REQUIRE(files.size() == 1);
    CHECK(files[0].suggested_name == "2025_videohandle.mp4");
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

TEST_CASE("ReviewFileNaming uses explicit folder-tree target directories")
{
    TempDir temp_dir;
    REQUIRE(std::filesystem::create_directories(temp_dir.path() / "10-19 Admin" / "11 Reports"));

    CategorizedFile file;
    file.file_path = temp_dir.path().string();
    file.file_name = "report.pdf";
    file.type = FileType::File;
    file.category = "10-19 Admin";
    file.subcategory = "11 Reports";
    file.folder_tree_mode = true;
    file.target_folder_relative_path = "10-19 Admin/11 Reports";

    const auto target =
        ReviewFileNaming::build_suggested_target_dir(file, temp_dir.path().string(), false);

    CHECK(target == temp_dir.path() / "10-19 Admin" / "11 Reports");
}

TEST_CASE("ReviewFileNaming uses the destination root for generated category folders")
{
    TempDir temp_dir;
    const std::filesystem::path destination_root = temp_dir.path() / "sorted";

    CategorizedFile file;
    file.file_path = (temp_dir.path() / "incoming").string();
    file.file_name = "report.pdf";
    file.type = FileType::File;
    file.category = "Documents";
    file.subcategory = "Reports";

    const auto target =
        ReviewFileNaming::build_suggested_target_dir(file, destination_root.string(), true);

    CHECK(target == destination_root / "Documents" / "Reports");
}
