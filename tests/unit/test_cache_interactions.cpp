#include <catch2/catch_test_macros.hpp>

#include "CategorizationService.hpp"
#include "DatabaseManager.hpp"
#include "FileScanner.hpp"
#include "ILLMClient.hpp"
#include "ResultsCoordinator.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

#include <atomic>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>

namespace {
void write_file(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "data";
}

class CountingLLM : public ILLMClient {
public:
    CountingLLM(std::shared_ptr<int> calls, std::string response)
        : calls_(std::move(calls)), response_(std::move(response)) {}

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string&) override {
        ++(*calls_);
        return response_;
    }

    std::string complete_prompt(const std::string&, int) override {
        return std::string();
    }

    void set_prompt_logging_enabled(bool) override {
    }

private:
    std::shared_ptr<int> calls_;
    std::string response_;
};
} // namespace

TEST_CASE("CategorizationService uses cached categorization without calling LLM") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    const std::string file_name = "cached.png";
    const auto resolved = db.resolve_category("Images", "Photos");
    REQUIRE(resolved.taxonomy_id > 0);
    REQUIRE(db.insert_or_update_file_with_categorization(
        file_name, "F", dir_path, resolved, false, std::string(), false));

    CategorizationService service(settings, db, nullptr);
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<CountingLLM>(calls, "Documents : Reports");
    };

    const auto full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Images");
    CHECK(categorized.front().subcategory == "Photos");
    CHECK(*calls == 0);
}

TEST_CASE("CategorizationService falls back to LLM when cache is empty") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    const std::string file_name = "uncached.txt";
    DatabaseManager::ResolvedCategory empty{0, "", ""};
    REQUIRE(db.insert_or_update_file_with_categorization(
        file_name, "F", dir_path, empty, false, std::string(), false));

    CategorizationService service(settings, db, nullptr);
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<CountingLLM>(calls, "Images : Photos");
    };

    const auto full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Images");
    CHECK(categorized.front().subcategory == "Photos");
    CHECK(*calls == 1);

    const auto cached = db.get_categorization_from_db(dir_path, file_name, FileType::File);
    REQUIRE(cached.size() == 2);
    CHECK(cached[0] == "Images");
    CHECK(cached[1] == "Photos");
}

TEST_CASE("CategorizationService loads cached entries recursively for analysis") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string root_path = data_dir.path().string();
    const std::string child_path = (data_dir.path() / "child").string();

    const auto resolved = db.resolve_category("Images", "Photos");
    REQUIRE(resolved.taxonomy_id > 0);

    REQUIRE(db.insert_or_update_file_with_categorization(
        "root.png", "F", root_path, resolved, false, std::string(), false));
    DatabaseManager::ResolvedCategory empty{0, "", ""};
    REQUIRE(db.insert_or_update_file_with_categorization(
        "suggested.png", "F", child_path, empty, false, "rename_me.png", true));

    settings.set_include_subdirectories(false);
    auto cached_root_only = service.load_cached_entries(root_path);
    REQUIRE(cached_root_only.size() == 1);
    CHECK(cached_root_only.front().file_name == "root.png");

    settings.set_include_subdirectories(true);
    auto cached_recursive = service.load_cached_entries(root_path);
    REQUIRE(cached_recursive.size() == 2);
    const auto it = std::find_if(cached_recursive.begin(), cached_recursive.end(),
                                 [](const CategorizedFile& entry) {
                                     return entry.file_name == "suggested.png";
                                 });
    REQUIRE(it != cached_recursive.end());
    CHECK(it->suggested_name == "rename_me.png");
    CHECK(it->file_path == child_path);
}

TEST_CASE("ResultsCoordinator respects full-path cache keys for recursive scans") {
    TempDir data_dir;
    const auto root_file = data_dir.path() / "sample.txt";
    const auto nested_file = data_dir.path() / "nested" / "sample.txt";
    write_file(root_file);
    write_file(nested_file);

    FileScanner scanner;
    ResultsCoordinator coordinator(scanner);
    const auto options = FileScanOptions::Files | FileScanOptions::Recursive;

    std::unordered_set<std::string> cached_by_name{"sample.txt"};
    auto uncached_by_name = coordinator.find_files_to_categorize(
        data_dir.path().string(),
        options,
        cached_by_name,
        false);
    CHECK(uncached_by_name.empty());

    std::unordered_set<std::string> cached_by_path{root_file.string()};
    auto uncached_by_path = coordinator.find_files_to_categorize(
        data_dir.path().string(),
        options,
        cached_by_path,
        true);
    REQUIRE(uncached_by_path.size() == 1);
    CHECK(uncached_by_path.front().full_path == nested_file.string());
}
