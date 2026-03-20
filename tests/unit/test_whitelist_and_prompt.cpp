#include <catch2/catch_test_macros.hpp>

#include "CategorizationService.hpp"
#include "CategorizationServiceTestAccess.hpp"
#include "DatabaseManager.hpp"
#include "ILLMClient.hpp"
#include "CategoryLanguage.hpp"
#include "LocalLLMTestAccess.hpp"
#include "Settings.hpp"
#include "WhitelistStore.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <atomic>
#include <memory>

namespace {
class FixedResponseLLM : public ILLMClient {
public:
    FixedResponseLLM(std::shared_ptr<int> calls, std::string response)
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

TEST_CASE("WhitelistStore initializes from settings and persists defaults") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_active_whitelist("MyList");

    WhitelistStore store(settings.get_config_dir());
    store.set("MyList", WhitelistEntry{{"Alpha", "Beta"}, {"One", "Two"}});
    store.save();

    store.initialize_from_settings(settings);

    auto names = store.list_names();
    REQUIRE(std::find(names.begin(), names.end(), "MyList") != names.end());
    auto entry = store.get("MyList");
    REQUIRE(entry.has_value());
    REQUIRE(entry->categories == std::vector<std::string>{"Alpha", "Beta"});
    REQUIRE(entry->subcategories == std::vector<std::string>{"One", "Two"});

    REQUIRE(settings.get_active_whitelist() == "MyList");
    REQUIRE(settings.get_allowed_categories() == entry->categories);
    REQUIRE(settings.get_allowed_subcategories() == entry->subcategories);

    WhitelistStore reloaded(settings.get_config_dir());
    REQUIRE(reloaded.load());
    auto persisted = reloaded.get("MyList");
    REQUIRE(persisted.has_value());
    REQUIRE(persisted->categories == entry->categories);
    REQUIRE(persisted->subcategories == entry->subcategories);
}

TEST_CASE("CategorizationService builds numbered whitelist context") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_allowed_categories({"CatA", "CatB"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_whitelist_context(service);

    REQUIRE(context.find("Allowed main categories") != std::string::npos);
    REQUIRE(context.find("1) CatA") != std::string::npos);
    REQUIRE(context.find("2) CatB") != std::string::npos);
    REQUIRE(context.find("Allowed subcategories: any") != std::string::npos);
}

TEST_CASE("CategorizationService builds category language context when non-English selected") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_category_language(CategoryLanguage::French);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_category_language_context(service);

    REQUIRE_FALSE(context.empty());
    REQUIRE(context.find("French") != std::string::npos);
}

TEST_CASE("CategorizationService builds category language context for Spanish") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_category_language(CategoryLanguage::Spanish);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_category_language_context(service);

    REQUIRE_FALSE(context.empty());
    REQUIRE(context.find("Spanish") != std::string::npos);
}

TEST_CASE("LocalLLM sanitizer keeps labeled multi-line replies intact") {
    const std::string output =
        "Category: Images\n"
        "Subcategory: Screenshots\n"
        "Reason: macOS screenshot naming pattern";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Images : Screenshots");
}

TEST_CASE("LocalLLM sanitizer prefers the last inline pair") {
    const std::string output =
        "Texts : Documents\n"
        "Productivity : File managers\n"
        "Archives : CAD assets";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Archives : CAD assets");
}

TEST_CASE("LocalLLM sanitizer strips rationale and natural language lead-ins") {
    const std::string output =
        "Based on the file name and context provided, the file falls under the Finances category : Credit reports";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Finances : Credit reports");
}

TEST_CASE("LocalLLM sanitizer ignores trailing note lines") {
    const std::string output =
        "Images : Screenshots\n"
        "(Note: Since the file is an image and not an installer, this question should not have been directed to me.)";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Images : Screenshots");
}

TEST_CASE("CategorizationService parses category output without spaced colon delimiters") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "report.xlsx";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Documents:Spreadsheets");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Spreadsheets");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService parses labeled category and subcategory lines") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "photo.jpg";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Category: Images\nSubcategory: Photos");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Images");
    CHECK(categorized.front().subcategory == "Photos");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService extracts the trailing pair from verbose responses") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "Screenshot 2026-03-10 at 12.07.00.png";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(
            calls,
            "Based on the filename and extension, the most appropriate categorization is: Images : Screenshots");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Images");
    CHECK(categorized.front().subcategory == "Screenshots");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService prefers the final pair when the model echoes examples") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "iphone14_pro_magsafe_stls.zip";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(
            calls,
            "Texts : Documents\n"
            "Productivity : File managers\n"
            "Archives : CAD assets");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Archives");
    CHECK(categorized.front().subcategory == "CAD assets");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService strips rationale from subcategory text") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "balenaEtcher-2.1.4-arm64.dmg";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(
            calls,
            "Operating system : MacOS (based on the .dmg file extension) - This file is an installer for macOS software");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Operating system");
    CHECK(categorized.front().subcategory == "MacOS");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService extracts a short category from natural language lead-ins") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "credit_excerpt.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(
            calls,
            "Based on the file name and context provided, the file falls under the Finances category : Credit reports");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Finances");
    CHECK(categorized.front().subcategory == "Credit reports");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService ignores trailing note lines after a valid answer") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "Capture d'ecran.png";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(
            calls,
            "Images : Screenshots\n"
            "(Note: Since the file is an image and not an installer, this question should not have been directed to me.)");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Images");
    CHECK(categorized.front().subcategory == "Screenshots");
    CHECK(*calls == 1);
}

TEST_CASE("CategorizationService progress shows current and categorization paths") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "legacy_name.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string suggested_name = "new_suggested_file_name.pdf";
    const std::string prompt_path =
        (data_dir.path() / suggested_name).generic_string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Category: Security\nSubcategory: PCI DSS guidelines");
    };

    std::vector<std::string> progress_messages;
    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        [&progress_messages](const std::string& message) { progress_messages.push_back(message); },
        {},
        {},
        {},
        factory,
        [suggested_name, prompt_path](const FileEntry&) {
            return CategorizationService::PromptOverride{suggested_name, prompt_path};
        });

    REQUIRE(categorized.size() == 1);
    REQUIRE(progress_messages.size() == 1);
    CHECK(progress_messages.front().find("Category            : Security") != std::string::npos);
    CHECK(progress_messages.front().find("Subcat              : PCI DSS guidelines") != std::string::npos);
    CHECK(progress_messages.front().find("Current Path        : " +
                                         Utils::abbreviate_user_path(full_path)) != std::string::npos);
    CHECK(progress_messages.front().find("Categorization Path : " +
                                         Utils::abbreviate_user_path(prompt_path)) != std::string::npos);
}
