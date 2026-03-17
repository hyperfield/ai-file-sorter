#include <catch2/catch_test_macros.hpp>
#include "FileScanner.hpp"
#include "TestHelpers.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static void write_file(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "data";
}

TEST_CASE("hidden files require explicit flag") {
    TempDir temp_dir;
    const auto hidden_file = temp_dir.path() / ".secret.txt";
    write_file(hidden_file);
#ifdef _WIN32
    auto current_attrs = GetFileAttributesW(hidden_file.c_str());
    if (current_attrs == INVALID_FILE_ATTRIBUTES) {
        FAIL("Failed to get attributes for hidden test file");
    }
    SetFileAttributesW(hidden_file.c_str(), current_attrs | FILE_ATTRIBUTE_HIDDEN);
#endif

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

TEST_CASE("recursive scans include nested files") {
    TempDir temp_dir;
    write_file(temp_dir.path() / "top.txt");
    write_file(temp_dir.path() / "nested" / "deep.txt");

    FileScanner scanner;
    const auto entries = scanner.get_directory_entries(
        temp_dir.path().string(),
        FileScanOptions::Files | FileScanOptions::Recursive);

    REQUIRE(entries.size() == 2);
    CHECK(std::any_of(entries.begin(), entries.end(), [](const FileEntry& entry) {
        return entry.file_name == "top.txt";
    }));
    CHECK(std::any_of(entries.begin(), entries.end(), [](const FileEntry& entry) {
        return entry.file_name == "deep.txt";
    }));
}

#ifndef _WIN32
class PermissionRestore {
public:
    explicit PermissionRestore(std::filesystem::path path)
        : path_(std::move(path))
    {
    }

    ~PermissionRestore() {
        std::error_code ec;
        std::filesystem::permissions(
            path_,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::add,
            ec);
    }

private:
    std::filesystem::path path_;
};

TEST_CASE("recursive scans skip unreadable directories and continue") {
    TempDir temp_dir;
    const auto readable_file = temp_dir.path() / "readable" / "keep.txt";
    const auto blocked_dir = temp_dir.path() / "blocked";
    const auto blocked_file = blocked_dir / "skip.txt";

    write_file(readable_file);
    write_file(blocked_file);
    PermissionRestore restore_permissions(blocked_dir);

    std::error_code ec;
    std::filesystem::permissions(
        blocked_dir,
        std::filesystem::perms::owner_all |
            std::filesystem::perms::group_all |
            std::filesystem::perms::others_all,
        std::filesystem::perm_options::remove,
        ec);
    REQUIRE(!ec);

    if (::access(blocked_dir.c_str(), R_OK | X_OK) == 0) {
        SKIP("permission restrictions are not enforced in this test environment");
    }

    FileScanner scanner;
    std::vector<FileEntry> entries;
    REQUIRE_NOTHROW(entries = scanner.get_directory_entries(
        temp_dir.path().string(),
        FileScanOptions::Files | FileScanOptions::Recursive));

    REQUIRE(entries.size() == 1);
    CHECK(entries.front().file_name == "keep.txt");
}
#endif
