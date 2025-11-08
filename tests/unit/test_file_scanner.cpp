#include <catch2/catch_test_macros.hpp>
#include "FileScanner.hpp"
#include "TestHelpers.hpp"
#include <fstream>
#include <filesystem>

static void write_file(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "data";
}

TEST_CASE("hidden files require explicit flag") {
    TempDir temp_dir;
    const auto hidden_file = temp_dir.path() / ".secret.txt";
    write_file(hidden_file);

    FileScanner scanner;
    auto entries = scanner.get_directory_entries(temp_dir.path().string(),
        FileScanOptions::Files);
    REQUIRE(entries.empty());

    entries = scanner.get_directory_entries(temp_dir.path().string(),
        FileScanOptions::Files | FileScanOptions::HiddenFiles);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().file_name == ".secret.txt");
    CHECK(entries.front().type == FileType::File);
}

TEST_CASE("junk files are skipped regardless of flags") {
    TempDir temp_dir;
    const auto junk_file = temp_dir.path() / ".DS_Store";
    write_file(junk_file);

    FileScanner scanner;
    auto entries = scanner.get_directory_entries(temp_dir.path().string(),
        FileScanOptions::Files | FileScanOptions::HiddenFiles);
    REQUIRE(entries.empty());
}

TEST_CASE("application bundles are treated as files") {
    TempDir temp_dir;
    const auto bundle_dir = temp_dir.path() / "Sample.app";
    std::filesystem::create_directories(bundle_dir / "Contents");

    FileScanner scanner;
    auto file_entries = scanner.get_directory_entries(temp_dir.path().string(),
        FileScanOptions::Files);
    REQUIRE(file_entries.size() == 1);
    CHECK(file_entries.front().file_name == "Sample.app");
    CHECK(file_entries.front().type == FileType::File);

    auto dir_entries = scanner.get_directory_entries(temp_dir.path().string(),
        FileScanOptions::Directories);
    REQUIRE(dir_entries.empty());
}
