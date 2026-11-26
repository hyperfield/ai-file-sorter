#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "LocalFsProvider.hpp"
#include "StorageProviderRegistry.hpp"
#include "TestHelpers.hpp"
#include "UndoManager.hpp"
#include "Utils.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {
namespace fs = std::filesystem;
using Catch::Matchers::ContainsSubstring;
constexpr auto kFixtureAge = std::chrono::hours(24);

void write_file(const fs::path& path, const std::string& contents = "fixture contents")
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
    REQUIRE(stream.good());
}

std::string read_file(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

LocalFileTransfer::Operations cross_device_operations()
{
    return {.rename = [](const fs::path&, const fs::path&, std::error_code& ec) {
        ec = std::make_error_code(std::errc::cross_device_link);
    }};
}

void check_no_staging_directories(const fs::path& parent)
{
    for (const auto& entry : fs::directory_iterator(parent)) {
        CHECK_FALSE(entry.path().filename().string().starts_with(".aifs-move-"));
    }
}
} // namespace

TEST_CASE("LocalFsProvider moves and undoes entries on the same filesystem", "[local-fs]")
{
    TempDir temp;
    const auto source = temp.path() / "Downloads" / "report.txt";
    const auto destination = temp.path() / "Library" / "Documents" / "report.txt";
    write_file(source);
    const LocalFsProvider provider;
    const auto result = provider.move_entry(source.string(), destination.string());
    INFO(result.message);
    REQUIRE(result.success);
    CHECK_FALSE(fs::exists(source));
    CHECK(read_file(destination) == "fixture contents");
    const auto restored = provider.undo_move(source.string(), destination.string(), {});
    INFO(restored.message);
    REQUIRE(restored.success);
    CHECK(read_file(source) == "fixture contents");
    CHECK_FALSE(fs::exists(destination));
}

TEST_CASE("LocalFsProvider copies files and folders across filesystems and supports undo", "[local-fs]")
{
    TempDir temp;
    const auto name = Utils::utf8_to_path("rapport été 東京");
    const auto source = temp.path() / "Downloads" / name;
    const auto library = temp.path() / "Library";
    const auto destination = library / "Sorted" / name;
    bool directory = false;
    SECTION("regular file") { write_file(source, std::string("data\0tail", 9)); }
    SECTION("whole folder including empty and hidden entries") {
        directory = true;
        write_file(source / "nested" / "report.txt", std::string("data\0tail", 9));
        write_file(source / ".hidden");
        fs::create_directories(source / "empty");
    }
    const auto payload = directory ? source / "nested" / "report.txt" : source;
    const auto copied_payload = directory ? destination / "nested" / "report.txt" : destination;
#ifndef _WIN32
    fs::permissions(payload, fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec |
                             fs::perms::group_read, fs::perm_options::replace);
#endif
    fs::last_write_time(payload, fs::file_time_type::clock::now() - kFixtureAge);
    const auto payload_time = fs::last_write_time(payload);
    const auto source_time = fs::last_write_time(source);
    const auto original_permissions = fs::status(payload).permissions();
    const auto root_permissions = fs::status(source).permissions();
    const LocalFsProvider provider{LocalFileTransfer{cross_device_operations()}};

    const auto result = provider.move_entry(Utils::path_to_utf8(source), Utils::path_to_utf8(destination));
    INFO(result.message);
    REQUIRE(result.success);
    CHECK_FALSE(fs::exists(source));
    CHECK(read_file(copied_payload) == std::string("data\0tail", 9));
    CHECK((fs::last_write_time(copied_payload) == payload_time));
    CHECK((fs::last_write_time(destination) == source_time));
    CHECK(fs::status(copied_payload).permissions() == original_permissions);
    CHECK(fs::status(destination).permissions() == root_permissions);
    CHECK(result.metadata.stable_identity == Utils::path_to_utf8(destination));
    if (directory) {
        CHECK(fs::is_empty(destination / "empty"));
        CHECK(fs::is_regular_file(destination / ".hidden"));
    } else {
        CHECK(result.metadata.size_bytes == fs::file_size(destination));
    }
    check_no_staging_directories(destination.parent_path());

    // Exercise the persisted undo checks as well as the provider's reverse transfer.
    QtAppContext qt;
    StorageProviderRegistry registry;
    registry.register_builtin(std::make_shared<LocalFsProvider>(provider));
    UndoManager undo((temp.path() / "undo").string(), &registry);
    REQUIRE(undo.save_plan(library.string(), provider.id(),
        {UndoManager::Entry{Utils::path_to_utf8(source), Utils::path_to_utf8(destination),
                            result.metadata.size_bytes, result.metadata.mtime,
                            result.metadata.stable_identity, result.metadata.revision_token}}, nullptr));
    const auto plan = undo.latest_plan_path();
    REQUIRE(plan.has_value());
    const auto restored = undo.undo_plan(*plan);
    INFO(restored.details.join("; ").toStdString());
    REQUIRE(restored.restored == 1);
    CHECK(restored.skipped == 0);
    CHECK_FALSE(fs::exists(destination));
    CHECK(read_file(payload) == std::string("data\0tail", 9));
    CHECK((fs::last_write_time(payload) == payload_time));
    check_no_staging_directories(source.parent_path());
}

TEST_CASE("LocalFsProvider preserves symlinks during cross-filesystem moves", "[local-fs]")
{
    TempDir temp;
    const auto source = temp.path() / "source";
    const auto destination = temp.path() / "library" / "moved";
    std::error_code ec;
    SECTION("dangling symlink") { fs::create_symlink("missing-target", source, ec); }
    SECTION("folder containing symlinks") {
        fs::create_directory(source);
        write_file(temp.path() / "outside", "external data");
        fs::create_symlink(temp.path() / "outside", source / "link", ec);
        if (!ec) fs::create_directory_symlink(".", source / "loop", ec);
    }
    if (ec) SKIP("Symlink creation unavailable: " + ec.message());
    const bool directory = fs::is_directory(fs::symlink_status(source));
    const LocalFsProvider provider{LocalFileTransfer{cross_device_operations()}};
    const auto moved = provider.move_entry(source.string(), destination.string());
    INFO(moved.message);
    REQUIRE(moved.success);
    CHECK_FALSE(provider.path_exists(source.string()));
    const auto link = directory ? destination / "link" : destination;
    REQUIRE(fs::is_symlink(fs::symlink_status(link)));
    CHECK(fs::read_symlink(link) == (directory ? temp.path() / "outside" : fs::path("missing-target")));
    if (directory) CHECK(fs::is_symlink(fs::symlink_status(destination / "loop")));
    const auto restored = provider.undo_move(source.string(), destination.string(), {});
    INFO(restored.message);
    REQUIRE(restored.success);
    CHECK(fs::is_symlink(fs::symlink_status(directory ? source / "link" : source)));
    if (directory) CHECK(read_file(temp.path() / "outside") == "external data");
}

TEST_CASE("LocalFsProvider only falls back for cross-device errors", "[local-fs]")
{
    TempDir temp;
    const auto source = temp.path() / "source.txt";
    const auto destination = temp.path() / "library" / "target.txt";
    write_file(source);
    auto operations = cross_device_operations();
    operations.rename = [](const fs::path&, const fs::path&, std::error_code& ec) {
        ec = std::make_error_code(std::errc::permission_denied);
    };
    operations.copy = [](const fs::path&, const fs::path&, std::error_code&) { FAIL("Unexpected copy"); };
    const LocalFsProvider provider{LocalFileTransfer{operations}};
    const auto result = provider.move_entry(source.string(), destination.string());
    CHECK_FALSE(result.success);
    CHECK(read_file(source) == "fixture contents");
    CHECK_FALSE(fs::exists(destination));
    check_no_staging_directories(destination.parent_path());
}

TEST_CASE("LocalFsProvider preserves the source and cleans staging after an incomplete copy", "[local-fs]")
{
    bool undo = false;
    SECTION("move") {}
    SECTION("undo") { undo = true; }
    TempDir temp;
    const auto source = temp.path() / "source";
    const auto destination = temp.path() / "library" / "target";
    write_file(source / "report.txt");
    auto operations = cross_device_operations();
    operations.copy = [](const fs::path&, const fs::path& staged, std::error_code& ec) {
        write_file(staged / "partial.txt", "incomplete");
        ec = std::make_error_code(std::errc::no_space_on_device);
    };
    const LocalFsProvider provider{LocalFileTransfer{operations}};
    const auto result = undo ? provider.undo_move(destination.string(), source.string(), {})
                             : provider.move_entry(source.string(), destination.string());
    CHECK_FALSE(result.success);
    CHECK(read_file(source / "report.txt") == "fixture contents");
    CHECK_FALSE(fs::exists(destination));
    check_no_staging_directories(destination.parent_path());
}

TEST_CASE("LocalFsProvider never overwrites destinations during cross-filesystem transfer", "[local-fs]")
{
    TempDir temp;
    const auto source = temp.path() / "source";
    const auto destination = temp.path() / "library" / "target";
    write_file(source);
    auto operations = cross_device_operations();
    SECTION("existing destination") { write_file(destination, "keep me"); }
    SECTION("destination appears while copying") {
        operations.copy = [&](const fs::path& from, const fs::path& staged, std::error_code& ec) {
            fs::copy_file(from, staged, ec);
            write_file(destination, "keep me");
        };
    }
    SECTION("empty directory appears while copying") {
        operations.copy = [&](const fs::path&, const fs::path& staged, std::error_code&) {
            fs::create_directory(staged);
            fs::create_directory(destination);
        };
    }
    const LocalFsProvider provider{LocalFileTransfer{operations}};
    const auto result = provider.move_entry(source.string(), destination.string());
    CHECK_FALSE(result.success);
    CHECK(read_file(source) == "fixture contents");
    if (fs::is_directory(destination)) CHECK(fs::is_empty(destination));
    else CHECK(read_file(destination) == "keep me");
    check_no_staging_directories(destination.parent_path());
}

TEST_CASE("LocalFsProvider retains the complete copy when source removal fails", "[local-fs]")
{
    bool undo = false;
    SECTION("move") {}
    SECTION("undo") { undo = true; }
    TempDir temp;
    const auto source = temp.path() / "source";
    const auto destination = temp.path() / "library" / "target";
    write_file(source / "first.txt", "first");
    write_file(source / "second.txt", "second");
    auto operations = cross_device_operations();
    operations.remove = [](const fs::path& from, std::error_code& ec) {
        fs::remove(from / "first.txt", ec); // A recursive removal can fail partway through.
        ec = std::make_error_code(std::errc::permission_denied);
    };
    const LocalFsProvider provider{LocalFileTransfer{operations}};
    const auto result = undo ? provider.undo_move(destination.string(), source.string(), {})
                             : provider.move_entry(source.string(), destination.string());
    CHECK_FALSE(result.success);
    CHECK_THAT(result.message, ContainsSubstring("Complete copy retained"));
    CHECK(read_file(source / "second.txt") == "second");
    CHECK(read_file(destination / "first.txt") == "first");
    CHECK(read_file(destination / "second.txt") == "second");
    check_no_staging_directories(destination.parent_path());
}

#ifndef _WIN32
TEST_CASE("LocalFsProvider rejects unsupported entries without deleting source data", "[local-fs]")
{
    TempDir temp;
    const auto source = temp.path() / "source";
    const auto destination = temp.path() / "library" / "target";
    write_file(source / "report.txt");
    REQUIRE(mkfifo((source / "pipe").c_str(), S_IRUSR | S_IWUSR) == 0);
    const LocalFsProvider provider{LocalFileTransfer{cross_device_operations()}};
    const auto result = provider.move_entry(source.string(), destination.string());
    CHECK_FALSE(result.success);
    CHECK(read_file(source / "report.txt") == "fixture contents");
    CHECK(fs::is_fifo(source / "pipe"));
    CHECK_FALSE(fs::exists(destination));
    check_no_staging_directories(destination.parent_path());
}
#endif
