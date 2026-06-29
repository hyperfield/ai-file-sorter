#include <catch2/catch_test_macros.hpp>

#include "CategorizationService.hpp"
#include "CategorizationServiceTestAccess.hpp"
#include "DatabaseManager.hpp"
#include "ILLMClient.hpp"
#include "LLMErrors.hpp"
#include "MainAppTestAccess.hpp"
#include "CategoryLanguage.hpp"
#include "LocalLLMTestAccess.hpp"
#include "Settings.hpp"
#include "TestHooks.hpp"
#include "UserLearningStore.hpp"
#include "WhitelistStore.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <QSettings>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
        ++(*calls_);
        return response_;
    }

    void set_prompt_logging_enabled(bool) override {
    }

private:
    std::shared_ptr<int> calls_;
    std::string response_;
};

class PromptCaptureLLM : public ILLMClient {
public:
    PromptCaptureLLM(std::shared_ptr<std::string> captured_path,
                     std::shared_ptr<std::string> captured_context,
                     std::shared_ptr<int> calls,
                     std::string response)
        : captured_path_(std::move(captured_path)),
          captured_context_(std::move(captured_context)),
          calls_(std::move(calls)),
          response_(std::move(response)) {}

    std::string categorize_file(const std::string&,
                                const std::string& file_path,
                                FileType,
                                const std::string& consistency_context) override {
        ++(*calls_);
        *captured_path_ = file_path;
        if (captured_context_) {
            *captured_context_ = consistency_context;
        }
        return response_;
    }

    std::string complete_prompt(const std::string& prompt, int) override {
        ++(*calls_);
        *captured_path_ = prompt;
        if (captured_context_) {
            *captured_context_ = prompt;
        }
        return response_;
    }

    void set_prompt_logging_enabled(bool) override {
    }

private:
    std::shared_ptr<std::string> captured_path_;
    std::shared_ptr<std::string> captured_context_;
    std::shared_ptr<int> calls_;
    std::string response_;
};

class TranslationAwareLLM : public ILLMClient {
public:
    TranslationAwareLLM(std::shared_ptr<int> categorize_calls,
                        std::shared_ptr<int> translation_calls,
                        std::string categorize_response,
                        std::deque<std::string> translation_responses)
        : categorize_calls_(std::move(categorize_calls)),
          translation_calls_(std::move(translation_calls)),
          categorize_response_(std::move(categorize_response)),
          translation_responses_(std::move(translation_responses)) {}

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string&) override {
        ++(*categorize_calls_);
        return categorize_response_;
    }

    std::string complete_prompt(const std::string& prompt, int) override {
        if (prompt.find("Translate the following taxonomy labels into") != std::string::npos) {
            ++(*translation_calls_);
            if (translation_responses_.empty()) {
                return std::string();
            }
            std::string response = translation_responses_.front();
            translation_responses_.pop_front();
            return response;
        }

        ++(*categorize_calls_);
        return categorize_response_;
    }

    void set_prompt_logging_enabled(bool) override {
    }

private:
    std::shared_ptr<int> categorize_calls_;
    std::shared_ptr<int> translation_calls_;
    std::string categorize_response_;
    std::deque<std::string> translation_responses_;
};

class BackoffThenResponseLLM : public ILLMClient {
public:
    explicit BackoffThenResponseLLM(std::shared_ptr<int> calls)
        : calls_(std::move(calls)) {}

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string&) override {
        ++(*calls_);
        if (*calls_ == 1) {
            throw BackoffError("rate limit", 2);
        }
        return "Documents : Reports";
    }

    std::string complete_prompt(const std::string&, int) override {
        return "Documents : Reports";
    }

    void set_prompt_logging_enabled(bool) override {
    }

private:
    std::shared_ptr<int> calls_;
};

class CategorizationSleepProbeGuard {
public:
    ~CategorizationSleepProbeGuard()
    {
        TestHooks::reset_categorization_sleep_probe();
    }
};

std::vector<std::string> document_topic_subcategories()
{
    return {
        "Invoices", "Receipts", "Taxes", "Contracts", "Reports", "Statements",
        "Letters", "Forms", "Certificates", "Policies", "Manuals", "Notes",
        "Presentations", "Spreadsheets", "Legal", "Insurance", "Banking"
    };
}

std::string join_for_settings(const std::vector<std::string>& values)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << values[i];
    }
    return out.str();
}

} // namespace

TEST_CASE("WhitelistStore seeds built-in presets when empty") {
    TempDir base_dir;
    WhitelistStore store(base_dir.path().string());

    REQUIRE(store.load());

    REQUIRE(store.list_names() == std::vector<std::string>{"Default", "Documents"});

    const auto default_entry = store.get("Default");
    REQUIRE(default_entry.has_value());
    CHECK(std::find(default_entry->categories.begin(),
                    default_entry->categories.end(),
                    "Audio") != default_entry->categories.end());
    CHECK(std::find(default_entry->categories.begin(),
                    default_entry->categories.end(),
                    "Music") == default_entry->categories.end());

    const auto documents = store.get("Documents");
    REQUIRE(documents.has_value());
    REQUIRE(documents->categories == std::vector<std::string>{"Documents"});
    REQUIRE(documents->subcategories.empty());
    REQUIRE(documents->subcategories_by_category.at("Documents") == document_topic_subcategories());
}

TEST_CASE("WhitelistStore migrates the Documents preset once for legacy stores") {
    TempDir base_dir;
    const auto legacy_path = base_dir.path() / "whitelists.ini";

    QSettings legacy_settings(QString::fromStdString(legacy_path.string()), QSettings::IniFormat);
    legacy_settings.beginGroup("Default");
    legacy_settings.setValue("Categories", "Alpha, Beta");
    legacy_settings.setValue("Subcategories", "One, Two");
    legacy_settings.endGroup();
    legacy_settings.sync();

    WhitelistStore store(base_dir.path().string());
    REQUIRE(store.load());

    const auto legacy_default = store.get("Default");
    REQUIRE(legacy_default.has_value());
    REQUIRE(legacy_default->categories == std::vector<std::string>{"Alpha", "Beta"});
    REQUIRE(legacy_default->subcategories == std::vector<std::string>{"One", "Two"});
    REQUIRE(store.get("Documents").has_value());

    store.remove("Documents");
    REQUIRE(store.save());

    WhitelistStore reloaded(base_dir.path().string());
    REQUIRE(reloaded.load());
    REQUIRE_FALSE(reloaded.get("Documents").has_value());
}

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

TEST_CASE("WhitelistStore migrates legacy Music categories to Audio") {
    TempDir base_dir;
    const auto legacy_path = base_dir.path() / "whitelists.ini";

    QSettings legacy_settings(QString::fromStdString(legacy_path.string()), QSettings::IniFormat);
    legacy_settings.beginGroup("__meta__");
    legacy_settings.setValue("BuiltInSeedVersion", 2);
    legacy_settings.endGroup();
    legacy_settings.beginGroup("Default");
    legacy_settings.setValue("Categories", "Music; Videos; Audio");
    legacy_settings.setValue("Subcategories", "");
    legacy_settings.endGroup();
    legacy_settings.sync();

    WhitelistStore store(base_dir.path().string());
    REQUIRE(store.load());

    const auto entry = store.get("Default");
    REQUIRE(entry.has_value());
    CHECK(entry->categories == std::vector<std::string>{"Audio", "Videos"});

    WhitelistStore reloaded(base_dir.path().string());
    REQUIRE(reloaded.load());
    const auto persisted = reloaded.get("Default");
    REQUIRE(persisted.has_value());
    CHECK(persisted->categories == std::vector<std::string>{"Audio", "Videos"});
}

TEST_CASE("WhitelistStore migrates legacy Documents preset to branching form") {
    TempDir base_dir;
    const auto legacy_path = base_dir.path() / "whitelists.ini";

    QSettings legacy_settings(QString::fromStdString(legacy_path.string()), QSettings::IniFormat);
    legacy_settings.beginGroup("__meta__");
    legacy_settings.setValue("BuiltInSeedVersion", 3);
    legacy_settings.endGroup();
    legacy_settings.beginGroup("Documents");
    legacy_settings.setValue("Categories", QString::fromStdString(join_for_settings(document_topic_subcategories())));
    legacy_settings.setValue("Subcategories", "");
    legacy_settings.endGroup();
    legacy_settings.sync();

    WhitelistStore store(base_dir.path().string());
    REQUIRE(store.load());

    const auto entry = store.get("Documents");
    REQUIRE(entry.has_value());
    CHECK(entry->categories == std::vector<std::string>{"Documents"});
    CHECK(entry->subcategories.empty());
    REQUIRE(entry->subcategories_by_category.at("Documents") == document_topic_subcategories());

    WhitelistStore reloaded(base_dir.path().string());
    REQUIRE(reloaded.load());
    const auto persisted = reloaded.get("Documents");
    REQUIRE(persisted.has_value());
    CHECK(persisted->categories == std::vector<std::string>{"Documents"});
    REQUIRE(persisted->subcategories_by_category.at("Documents") == document_topic_subcategories());
}

TEST_CASE("WhitelistStore preserves Unicode labels through save and load") {
    TempDir base_dir;
    const WhitelistEntry entry{
        {"Research ☁️", "Manuals"},
        {"AB Testing ☁️", "Abroad ☁️"}
    };

    WhitelistStore store(base_dir.path().string());
    store.set("Cloud ☁️", entry);
    REQUIRE(store.save());

    WhitelistStore reloaded(base_dir.path().string());
    REQUIRE(reloaded.load());

    const auto persisted = reloaded.get("Cloud ☁️");
    REQUIRE(persisted.has_value());
    CHECK(persisted->categories == entry.categories);
    CHECK(persisted->subcategories == entry.subcategories);

    const auto names = reloaded.list_names();
    CHECK(std::find(names.begin(), names.end(), "Cloud ☁️") != names.end());
}

TEST_CASE("WhitelistStore preserves branching subcategories through save and settings initialization") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_active_whitelist("Smart");

    const WhitelistEntry entry{
        {"Documents", "Images"},
        {},
        {
            {"Documents", {"Invoices", "Receipts"}},
            {"Images", {"Screenshots", "Photos"}}
        }
    };

    WhitelistStore store(settings.get_config_dir());
    store.set("Smart", entry);
    REQUIRE(store.save());

    WhitelistStore reloaded(settings.get_config_dir());
    REQUIRE(reloaded.load());
    const auto persisted = reloaded.get("Smart");
    REQUIRE(persisted.has_value());
    CHECK(persisted->categories == entry.categories);
    CHECK(persisted->subcategories.empty());
    CHECK(persisted->subcategories_by_category == entry.subcategories_by_category);

    reloaded.initialize_from_settings(settings);
    CHECK(settings.get_allowed_categories() == entry.categories);
    CHECK(settings.get_allowed_subcategories().empty());
    CHECK(settings.get_allowed_subcategories_by_category() == entry.subcategories_by_category);
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

TEST_CASE("CategorizationService builds branching whitelist context") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_allowed_categories({"Documents", "Images"});
    settings.set_allowed_subcategories({});
    settings.set_allowed_subcategories_by_category({
        {"Documents", {"Invoices", "Receipts"}},
        {"Images", {"Screenshots", "Photos"}}
    });
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_whitelist_context(service);

    CHECK(context.find("Allowed main categories") != std::string::npos);
    CHECK(context.find("Allowed subcategories by main category") != std::string::npos);
    CHECK(context.find("Documents : Invoices, Receipts") != std::string::npos);
    CHECK(context.find("Images : Screenshots, Photos") != std::string::npos);
    CHECK(context.find("Allowed subcategories (pick exactly one label") == std::string::npos);
}

TEST_CASE("CategorizationService preserves Unicode whitelist labels in combined context") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Business ☁️"});
    settings.set_allowed_subcategories({"AB Testing ☁️", "Abroad ☁️"});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        "sample.pdf",
        (base_dir.path() / "sample.pdf").string(),
        FileType::File);

    CHECK(context.find("1) Business ☁️") != std::string::npos);
    CHECK(context.find("1) AB Testing ☁️") != std::string::npos);
    CHECK(context.find("2) Abroad ☁️") != std::string::npos);
}

TEST_CASE("CategorizationService corrects invalid branching whitelist subcategory pairs") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Documents", "Images"});
    settings.set_allowed_subcategories({});
    settings.set_allowed_subcategories_by_category({
        {"Documents", {"Invoices", "Receipts"}},
        {"Images", {"Screenshots", "Photos"}}
    });
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "invoice.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Documents : Screenshots");
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
    CHECK(categorized.front().canonical_category == "Documents");
    CHECK(categorized.front().canonical_subcategory == "Invoices");
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Invoices");
}

TEST_CASE("CategorizationService keeps small whitelists fully injected") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Manuals", "Spreadsheets"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        "camera_manual.pdf",
        (base_dir.path() / "camera_manual.pdf").string(),
        FileType::File);

    CHECK(context.find("Allowed main categories") != std::string::npos);
    CHECK(context.find("1) Manuals") != std::string::npos);
    CHECK(context.find("2) Spreadsheets") != std::string::npos);
    CHECK(context.find("Selected whitelist is large") == std::string::npos);
}

TEST_CASE("CategorizationService retrieves large branching whitelist candidates") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);

    std::vector<std::string> categories;
    std::unordered_map<std::string, std::vector<std::string>> mapped_subcategories;
    for (int i = 0; i < 18; ++i) {
        const auto category = "Project Bucket " + std::to_string(i);
        categories.push_back(category);
        mapped_subcategories[category] = {"Alpha", "Beta"};
    }
    categories.push_back("Documents");
    mapped_subcategories["Documents"] = {"Invoices", "Receipts"};
    settings.set_allowed_categories(categories);
    settings.set_allowed_subcategories({});
    settings.set_allowed_subcategories_by_category(mapped_subcategories);

    DatabaseManager db(settings.get_config_dir());
    UserLearningStore learning_store(settings.get_config_dir());
    REQUIRE(learning_store.is_open());
    std::string error;
    REQUIRE(learning_store.import_taxonomy_candidates({{"Documents", "Receipts", "whitelist:Smart"}}, &error));

    CategorizationService service(settings, db, nullptr, &learning_store);
    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        "invoice_receipt.pdf",
        (base_dir.path() / "invoice_receipt.pdf").string(),
        FileType::File);

    CHECK(context.find("Selected whitelist is large") != std::string::npos);
    CHECK(context.find("Documents : Receipts") != std::string::npos);
    CHECK(context.find("Allowed subcategories for shown category candidates") != std::string::npos);
    CHECK(context.find("Documents: Invoices, Receipts") != std::string::npos);
    CHECK(context.find("Project Bucket 17") == std::string::npos);
}

TEST_CASE("CategorizationService retrieves candidates instead of injecting large whitelists") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);

    std::vector<std::string> categories;
    for (int i = 0; i < 35; ++i) {
        categories.push_back("Archive Bucket " + std::to_string(i));
    }
    categories.push_back("Manuals");
    categories.push_back("Spreadsheets");
    settings.set_allowed_categories(categories);
    settings.set_allowed_subcategories({});

    DatabaseManager db(settings.get_config_dir());
    UserLearningStore learning_store(settings.get_config_dir());
    REQUIRE(learning_store.is_open());
    std::string error;
    REQUIRE(learning_store.import_taxonomy_candidates({{"Manuals", "", "whitelist:Default"},
                                                       {"Spreadsheets", "", "whitelist:Default"}},
                                                      &error));

    CategorizationService service(settings, db, nullptr, &learning_store);
    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        "camera_manual.pdf",
        (base_dir.path() / "camera_manual.pdf").string(),
        FileType::File);

    CHECK(context.find("Selected whitelist is large") != std::string::npos);
    CHECK(context.find("Allowed category candidates") != std::string::npos);
    CHECK(context.find("Manuals") != std::string::npos);
    CHECK(context.find("Archive Bucket 34") == std::string::npos);
    CHECK(context.find("Allowed main categories") == std::string::npos);
    CHECK(context.find("User-learned category candidates") == std::string::npos);
}

TEST_CASE("CategorizationService ranks large whitelist candidates without learning store") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);

    std::vector<std::string> categories;
    for (int i = 0; i < 35; ++i) {
        categories.push_back("Archive Bucket " + std::to_string(i));
    }
    categories.push_back("Manuals");
    settings.set_allowed_categories(categories);
    settings.set_allowed_subcategories({});

    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);
    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        "camera_manual.pdf",
        (base_dir.path() / "camera_manual.pdf").string(),
        FileType::File);

    CHECK(context.find("Selected whitelist is large") != std::string::npos);
    CHECK(context.find("Manuals") != std::string::npos);
    CHECK(context.find("Archive Bucket 34") == std::string::npos);
}

TEST_CASE("CategorizationService adds relevant learned taxonomy candidates to context") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    UserLearningStore learning_store(settings.get_config_dir());
    REQUIRE(learning_store.is_open());

    std::string error;
    UserLearningStore::ApprovedMapping mapping;
    mapping.file_name = "nikon_camera_manual.pdf";
    mapping.file_type = FileType::File;
    mapping.dir_path = "/docs/cameras";
    mapping.category = "Manuals";
    mapping.subcategory = "Camera Guides";
    REQUIRE(learning_store.record_approved_mapping(mapping, &error));
    REQUIRE(learning_store.import_taxonomy_candidates({{"Spreadsheets", "", "whitelist:Default"}}, &error));

    CategorizationService service(settings, db, nullptr, &learning_store);
    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        "camera_setup_manual.pdf",
        (base_dir.path() / "camera_setup_manual.pdf").string(),
        FileType::File);

    CHECK(context.find("User-learned category candidates") != std::string::npos);
    CHECK(context.find("Documents : Camera Guides") != std::string::npos);
}

TEST_CASE("CategorizationService prefers learned candidates over generic model categories") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    UserLearningStore learning_store(settings.get_config_dir());
    REQUIRE(learning_store.is_open());

    std::string error;
    UserLearningStore::ApprovedMapping mapping;
    mapping.file_name = "nikon_camera_manual.pdf";
    mapping.file_type = FileType::File;
    mapping.dir_path = "/docs/cameras";
    mapping.category = "Manuals";
    mapping.subcategory = "Camera Guides";
    mapping.context_text = "Camera aperture setup and lens maintenance instructions.";
    REQUIRE(learning_store.record_approved_mapping(mapping, &error));

    CategorizationService service(settings, db, nullptr, &learning_store);
    TempDir data_dir;
    const std::string file_name = "camera_setup_manual.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Category: Documents\nSubcategory: General");
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
    CHECK(categorized.front().canonical_category == "Documents");
    CHECK(categorized.front().canonical_subcategory == "Camera Guides");
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Camera Guides");
}

TEST_CASE("CategorizationService ignores whitelist-imported candidates when the model already returns a specific document subcategory") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    UserLearningStore learning_store(settings.get_config_dir());
    REQUIRE(learning_store.is_open());

    std::string error;
    REQUIRE(learning_store.import_taxonomy_candidates({{"Documents", "Payments", "whitelist:Default"}}, &error));

    CategorizationService service(settings, db, nullptr, &learning_store);
    TempDir data_dir;
    const std::string file_name = "payment_receipt.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Documents : Receipts");
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
    CHECK(categorized.front().canonical_category == "Documents");
    CHECK(categorized.front().canonical_subcategory == "Receipts");
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Receipts");
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
    REQUIRE(context.find("English only") != std::string::npos);
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
    REQUIRE(context.find("English only") != std::string::npos);
    REQUIRE(context.find("Spanish") != std::string::npos);
}

TEST_CASE("CategorizationService retries remote categorization once after BackoffError") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_llm_choice(LLMChoice::Remote_OpenAI);
    settings.set_openai_api_key("test-key");
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    CategorizationSleepProbeGuard sleep_guard;
    std::vector<std::chrono::milliseconds> sleep_durations;
    TestHooks::set_categorization_sleep_probe(
        [&sleep_durations](std::chrono::milliseconds duration) {
            sleep_durations.push_back(duration);
        });

    TempDir data_dir;
    const std::string file_name = "remote_retry.txt";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};
    std::vector<std::string> progress_messages;
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<BackoffThenResponseLLM>(calls);
    };

    const auto categorized = service.categorize_entries(
        files,
        false,
        stop_flag,
        [&progress_messages](const std::string& message) {
            progress_messages.push_back(message);
        },
        {},
        {},
        {},
        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(*calls == 2);
    CHECK(categorized.front().canonical_category == "Documents");
    CHECK(categorized.front().canonical_subcategory == "Reports");
    REQUIRE(sleep_durations.size() == 2);
    CHECK(sleep_durations[0] == std::chrono::seconds(1));
    CHECK(sleep_durations[1] == std::chrono::seconds(1));
    CHECK(std::any_of(progress_messages.begin(), progress_messages.end(), [](const std::string& message) {
        return message.find("Rate limit hit") != std::string::npos;
    }));
    CHECK(std::any_of(progress_messages.begin(), progress_messages.end(), [](const std::string& message) {
        return message.find("Retrying remote_retry.txt in 2s") != std::string::npos;
    }));
}

TEST_CASE("CategorizationService resolves remote request pacing from settings and environment") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    EnvVarGuard limit_guard("AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE", std::nullopt);
    Settings settings;
    settings.set_remote_requests_per_minute(20);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    CHECK(CategorizationServiceTestAccess::resolve_remote_requests_per_minute(service) == 20);

    EnvVarGuard override_guard("AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE", std::string("7"));
    CHECK(CategorizationServiceTestAccess::resolve_remote_requests_per_minute(service) == 7);

    EnvVarGuard disabled_guard("AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE", std::string("0"));
    CHECK(CategorizationServiceTestAccess::resolve_remote_requests_per_minute(service) == 0);
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

TEST_CASE("LocalLLM sanitizer strips translated parenthetical glosses") {
    const std::string output =
        "Traitement de texte. (Text documents) : Installateurs. (Software installation)";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Traitement de texte : Installateurs");
}

TEST_CASE("LocalLLM sanitizer strips inline subcategory label artifacts from category values") {
    const std::string output = "Category: Images, subcategory: Funny seals";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Images : Funny seals");
}

TEST_CASE("LocalLLM sanitizer strips spaced inline subcategory artifacts from category values") {
    const std::string output = "Category: Documents , subcategory Fichiers en Francais";

    REQUIRE(LocalLLMTestAccess::sanitize_output_for_testing(output) == "Documents");
}

TEST_CASE("CategorizationService parses category output without spaced colon delimiters") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "invoice.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Documents:Invoices");
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
    CHECK(categorized.front().subcategory == "Invoices");
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
        return std::make_unique<FixedResponseLLM>(calls, "Category: Images\nSubcategory: Seal Portraits");
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
    CHECK(categorized.front().subcategory == "Seal Portraits");
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
            "Based on the filename and extension, the most appropriate categorization is: Images : Dashboard Interfaces");
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
    CHECK(categorized.front().subcategory == "Dashboard Interfaces");
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
    CHECK(categorized.front().category == "Operating Systems");
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
    CHECK(categorized.front().category == "Documents");
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
            "Images : UI Dashboards\n"
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
    CHECK(categorized.front().subcategory == "UI Dashboards");
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
    CHECK(progress_messages.front().find("Category            : Documents") != std::string::npos);
    CHECK(progress_messages.front().find("Subcat              : PCI DSS guidelines") != std::string::npos);
    CHECK(progress_messages.front().find("Current Path        : " +
                                         Utils::abbreviate_user_path(full_path)) != std::string::npos);
    CHECK(progress_messages.front().find("Categorization Path : " +
                                         Utils::abbreviate_user_path(prompt_path)) != std::string::npos);
}

TEST_CASE("Document prompt helpers use the suggested filename for categorization") {
    CHECK(MainAppTestAccess::resolve_document_prompt_name("legacy_name.pdf", "cached_name.pdf") ==
          "cached_name.pdf");
    CHECK(MainAppTestAccess::resolve_document_prompt_name("legacy_name.pdf", "") ==
          "legacy_name.pdf");
}

TEST_CASE("Document prompt path uses the suggested filename and preserves summaries") {
    TempDir data_dir;
    const std::filesystem::path original_path = data_dir.path() / "legacy_name.pdf";
    const std::string prompt_name = "new_suggested_name.pdf";
    const std::string summary = "PCI DSS quick reference";
    const std::string expected_path = Utils::path_to_utf8(data_dir.path() / prompt_name);
    const std::string prompt_path = MainAppTestAccess::build_document_prompt_path(
        original_path.string(),
        prompt_name,
        summary);

    CHECK(prompt_path.find(expected_path) == 0);
    CHECK(prompt_path.find("\nDocument summary: " + summary) != std::string::npos);
    CHECK(prompt_path.find("legacy_name.pdf") == std::string::npos);
}

TEST_CASE("Image prompt path uses the suggested filename and preserves descriptions") {
    TempDir data_dir;
    const std::filesystem::path original_path = data_dir.path() / "legacy_name.jpg";
    const std::string prompt_name = "snowy_mountain.jpg";
    const std::string description = "A snow-covered mountain under a clear blue sky.";
    const std::string expected_path = Utils::path_to_utf8(data_dir.path() / prompt_name);
    const std::string prompt_path = MainAppTestAccess::build_image_prompt_path(
        original_path.string(),
        prompt_name,
        description);

    CHECK(prompt_path.find(expected_path) == 0);
    CHECK(prompt_path.find("\nImage description: " + description) != std::string::npos);
    CHECK(prompt_path.find("legacy_name.jpg") == std::string::npos);
}

TEST_CASE("CategorizationService passes image descriptions through prompt overrides") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "legacy_name.jpg";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string suggested_name = "snowy_mountain.jpg";
    const std::string description = "A snow-covered mountain under a clear blue sky.";
    const std::string prompt_path = MainAppTestAccess::build_image_prompt_path(
        full_path,
        suggested_name,
        description);
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto captured_path = std::make_shared<std::string>();
    auto factory = [captured_path, calls]() {
        return std::make_unique<PromptCaptureLLM>(
            captured_path,
            std::shared_ptr<std::string>{},
            calls,
            "Category: Nature\nSubcategory: Mountains");
    };

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        {},
        factory,
        [suggested_name, prompt_path](const FileEntry&) {
            return CategorizationService::PromptOverride{suggested_name, prompt_path};
        });

    REQUIRE(categorized.size() == 1);
    CHECK(*calls == 1);
    CHECK(captured_path->find(suggested_name) != std::string::npos);
    CHECK(captured_path->find("\nImage description: " + description) != std::string::npos);
    CHECK(captured_path->find("legacy_name.jpg") == std::string::npos);
}

TEST_CASE("CategorizationService preserves analysis context for learned behavior capture") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "legacy_name.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string suggested_name = "camera_setup_guide.pdf";
    const std::string summary = "Camera setup and maintenance instructions.";
    const std::string prompt_path = MainAppTestAccess::build_document_prompt_path(
        full_path,
        suggested_name,
        summary);
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Category: Manuals\nSubcategory: Camera Guides");
    };

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        {},
        factory,
        [suggested_name, prompt_path](const FileEntry&) {
            return CategorizationService::PromptOverride{suggested_name, prompt_path};
        });

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().learning_context == "Document summary: " + summary);
}

TEST_CASE("CategorizationService adds stable guidance for supported document prompts") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string prompt_name = "pci_dss_quick_reference.pdf";
    const std::string prompt_path = MainAppTestAccess::build_document_prompt_path(
        (base_dir.path() / prompt_name).string(),
        prompt_name,
        "Quick reference to PCI DSS controls for merchants.");

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        prompt_name,
        prompt_path,
        FileType::File);

    CHECK(context.find("Document categorization guidance:") != std::string::npos);
    CHECK(context.find("Prefer one of these main categories when it clearly fits: Documents, Presentations, Spreadsheets, Data Exports, Configs.") != std::string::npos);
    CHECK(context.find("Put the specific topic or subject matter in the subcategory") != std::string::npos);
    CHECK(context.find("Allowed main categories") != std::string::npos);
    CHECK(context.find("1) Documents") != std::string::npos);
    CHECK(context.find("2) Presentations") != std::string::npos);
    CHECK(context.find("5) Configs") != std::string::npos);

    const std::string legacy_prompt_name = "file-sample_100kB.doc";
    const std::string legacy_prompt_path = (base_dir.path() / legacy_prompt_name).string();
    const std::string legacy_context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        legacy_prompt_name,
        legacy_prompt_path,
        FileType::File);

    CHECK(legacy_context.find("Document categorization guidance:") != std::string::npos);
    CHECK(legacy_context.find("Prefer one of these main categories when it clearly fits: Documents, Presentations, Spreadsheets, Data Exports, Configs.") != std::string::npos);
    CHECK(legacy_context.find("Allowed main categories") != std::string::npos);
}

TEST_CASE("CategorizationService normalizes supported document main categories to stable buckets") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    struct TestCase {
        const char* file_name;
        const char* summary;
        const char* response;
        const char* expected_category;
        const char* expected_subcategory;
    };

    const std::vector<TestCase> cases = {
        {"pci_dss_quick_reference.pdf",
         "Quick reference to PCI DSS controls and security requirements.",
         "Computing : Security Compliance",
         "Documents",
         "Security Compliance"},
        {"product_launch_deck.pptx",
         "Presentation slides for a new product launch.",
         "Marketing : Product Launch",
         "Presentations",
         "Product Launch"},
        {"quarterly_budget.xlsx",
         "Workbook with quarterly budget totals by department.",
         "Finance : Quarterly Budgets",
         "Spreadsheets",
         "Quarterly Budgets"},
        {"sales_export.csv",
         "CSV export of sales metrics by region.",
         "Analytics : Regional Metrics",
         "Data Exports",
         "Regional Metrics"},
        {"desktop_preferences.conf",
         "Configuration file for desktop preferences.",
         "System : Desktop Preferences",
         "Configs",
         "Desktop Preferences"},
        {"file-sample_100kB.doc",
         nullptr,
         "Office Software : Meeting Minutes",
         "Documents",
         "Meeting Minutes"},
        {"quarterly_budget_legacy.xls",
         nullptr,
         "Finance : Department Budget",
         "Spreadsheets",
         "Department Budget"},
        {"sales_kickoff_legacy.ppt",
         nullptr,
         "Marketing : Sales Kickoff",
         "Presentations",
         "Sales Kickoff"}
    };

    for (const auto& test_case : cases) {
        TempDir data_dir;
        const std::string full_path = (data_dir.path() / test_case.file_name).string();
        const std::string prompt_path = test_case.summary
            ? MainAppTestAccess::build_document_prompt_path(
                  full_path,
                  test_case.file_name,
                  test_case.summary)
            : full_path;
        const std::vector<FileEntry> files = {FileEntry{full_path, test_case.file_name, FileType::File}};

        std::atomic<bool> stop_flag{false};
        auto calls = std::make_shared<int>(0);
        auto factory = [calls, &test_case]() {
            return std::make_unique<FixedResponseLLM>(calls, test_case.response);
        };

        const auto categorized = service.categorize_entries(
            files,
            true,
            stop_flag,
            {},
            {},
            {},
            {},
            factory,
            [&test_case, &prompt_path](const FileEntry&) {
                return CategorizationService::PromptOverride{test_case.file_name, prompt_path};
            });

        CAPTURE(test_case.file_name);
        REQUIRE(categorized.size() == 1);
        CHECK(categorized.front().canonical_category == test_case.expected_category);
        CHECK(categorized.front().canonical_subcategory == test_case.expected_subcategory);
    }
}

TEST_CASE("CategorizationService preserves explicit whitelist document main categories") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Contracts", "Policies"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "vendor_agreement.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string prompt_path = MainAppTestAccess::build_document_prompt_path(
        full_path,
        file_name,
        "Vendor agreement covering support and maintenance obligations.");
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Contracts : Vendor Services");
    };

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        {},
        factory,
        [file_name, prompt_path](const FileEntry&) {
            return CategorizationService::PromptOverride{file_name, prompt_path};
        });

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().canonical_category == "Contracts");
    CHECK(categorized.front().canonical_subcategory == "Vendor Services");
}

TEST_CASE("CategorizationService adds stable guidance for supported image prompts") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string prompt_name = "seal_photo.jpg";
    const std::string prompt_path = (base_dir.path() / prompt_name).string();

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        prompt_name,
        prompt_path,
        FileType::File);

    CHECK(context.find("Image categorization guidance:") != std::string::npos);
    CHECK(context.find("Always use Images as the main category") != std::string::npos);
    CHECK(context.find("put the depicted subject, scene, or on-screen content in the subcategory") != std::string::npos);
    CHECK(context.find("Allowed main categories") != std::string::npos);
    CHECK(context.find("1) Images") != std::string::npos);
}

TEST_CASE("CategorizationService leaves generic software-like prompts unscaffolded without whitelist") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string prompt_name = "Git-2.50.0.2-64-bit.exe";
    const std::string prompt_path = (base_dir.path() / prompt_name).string();

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        prompt_name,
        prompt_path,
        FileType::File);

    CHECK(context.empty());
}

TEST_CASE("CategorizationService adds artifact guidance for software-like prompts only when whitelist mode is active") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Software", "Installers", "Drivers", "Operating Systems"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string prompt_name = "Git-2.50.0.2-64-bit.exe";
    const std::string prompt_path = (base_dir.path() / prompt_name).string();

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        prompt_name,
        prompt_path,
        FileType::File);

    CHECK(context.find("Software and archive artifact guidance:") != std::string::npos);
    CHECK(context.find("Allowed main categories") != std::string::npos);
    CHECK(context.find("1) Software") != std::string::npos);
    CHECK(context.find("2) Installers") != std::string::npos);
    CHECK(context.find("Installer Tools") != std::string::npos);
    CHECK(context.find("Installer Builders") == std::string::npos);
}

TEST_CASE("CategorizationService structurally canonicalizes artifact family labels without semantic remapping") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    struct TestCase {
        const char* file_name;
        const char* response;
        const char* expected_category;
        const char* expected_subcategory;
    };

    const std::vector<TestCase> cases = {
        {"Git-2.50.0.2-64-bit.exe",
         "Software : Version Control",
         "Software",
         "Version Control"},
        {"nsis-3.11-setup.exe",
         "Installer : Installer",
         "Installers",
         "General"},
        {"VMware-workstation-full-17.6.4-24832109.exe",
         "Operating Systems : Virtualization Software",
         "Operating Systems",
         "Virtualization Software"},
        {"581.57-desktop-win10-win11-64bit-international-nsd-dch-whql.exe",
         "Driver : Driver",
         "Drivers",
         "General"}
    };

    for (const auto& test_case : cases) {
        TempDir data_dir;
        const std::string full_path = (data_dir.path() / test_case.file_name).string();
        const std::vector<FileEntry> files = {FileEntry{full_path, test_case.file_name, FileType::File}};

        std::atomic<bool> stop_flag{false};
        auto calls = std::make_shared<int>(0);
        auto factory = [calls, &test_case]() {
            return std::make_unique<FixedResponseLLM>(calls, test_case.response);
        };

        const auto categorized = service.categorize_entries(
            files,
            true,
            stop_flag,
            {},
            {},
            {},
            {},
            factory);

        CAPTURE(test_case.file_name);
        REQUIRE(categorized.size() == 1);
        CHECK(categorized.front().canonical_category == test_case.expected_category);
        CHECK(categorized.front().canonical_subcategory == test_case.expected_subcategory);
    }
}

TEST_CASE("CategorizationService preserves non-family artifact semantics from the model output") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    struct TestCase {
        const char* file_name;
        const char* response;
        const char* expected_category;
        const char* expected_subcategory;
    };

    const std::vector<TestCase> cases = {
        {"sqlite-tools-win-x64-3500400.zip",
         "Database Management Tools : Database Management Systems",
         "Database Management Tools",
         "Database Management Systems"},
        {"ProcessMonitor.zip",
         "System Utilities : Process Monitoring",
         "System Utilities",
         "Process Monitoring"},
        {"sales_export_bundle.zip",
         "Analytics : Regional Metrics",
         "Analytics",
         "Regional Metrics"},
        {"cad_assets_bundle.zip",
         "Archive : CAD assets",
         "Archives",
         "CAD assets"}
    };

    for (const auto& test_case : cases) {
        TempDir data_dir;
        const std::string full_path = (data_dir.path() / test_case.file_name).string();
        const std::vector<FileEntry> files = {FileEntry{full_path, test_case.file_name, FileType::File}};

        std::atomic<bool> stop_flag{false};
        auto calls = std::make_shared<int>(0);
        auto factory = [calls, &test_case]() {
            return std::make_unique<FixedResponseLLM>(calls, test_case.response);
        };

        const auto categorized = service.categorize_entries(
            files,
            true,
            stop_flag,
            {},
            {},
            {},
            {},
            factory);

        CAPTURE(test_case.file_name);
        REQUIRE(categorized.size() == 1);
        CHECK(categorized.front().canonical_category == test_case.expected_category);
        CHECK(categorized.front().canonical_subcategory == test_case.expected_subcategory);
    }
}

TEST_CASE("CategorizationService falls back to General when a broad family subcategory repeats the category") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "podcast_episode.mp3";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Audio : Audio");
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
    CHECK(categorized.front().canonical_category == "Audio");
    CHECK(categorized.front().canonical_subcategory == "General");
    CHECK(categorized.front().category == "Audio");
    CHECK(categorized.front().subcategory == "General");
}

TEST_CASE("CategorizationService preserves explicit whitelist software-like main categories") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Utilities", "Drivers"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "ProcessMonitor.zip";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Utilities : Process Monitoring");
    };

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        {},
        factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().canonical_category == "Utilities");
    CHECK(categorized.front().canonical_subcategory == "Process Monitoring");
}

TEST_CASE("CategorizationService leaves unknown generic prompts unscaffolded without whitelist") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string prompt_name = "mystery.asset";
    const std::string prompt_path = (base_dir.path() / prompt_name).string();

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        prompt_name,
        prompt_path,
        FileType::File);

    CHECK(context.empty());
}

TEST_CASE("CategorizationService uses one-shot categorization prompts for document files") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "pci_dss_quick_reference.pdf";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string prompt_path = MainAppTestAccess::build_document_prompt_path(
        full_path,
        file_name,
        "Quick reference to PCI DSS controls for merchants and service providers.");
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto captured_path = std::make_shared<std::string>();
    auto captured_context = std::make_shared<std::string>();
    auto factory = [captured_path, captured_context, calls]() {
        return std::make_unique<PromptCaptureLLM>(
            captured_path,
            captured_context,
            calls,
            "Documents : PCI DSS");
    };

    const auto categorized = service.categorize_entries(files,
                                                        true,
                                                        stop_flag,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory,
                                                        [file_name, prompt_path](const FileEntry&) {
                                                            return CategorizationService::PromptOverride{file_name, prompt_path};
                                                        });

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().canonical_category == "Documents");
    CHECK(categorized.front().canonical_subcategory == "PCI DSS");
    CHECK(*calls == 1);
    CHECK(captured_path->find(file_name) != std::string::npos);
    CHECK(captured_path->find("Document summary: Quick reference to PCI DSS controls for merchants and service providers.") != std::string::npos);
    CHECK(captured_context->find("Document categorization guidance:") != std::string::npos);
    CHECK(captured_context->find("Allowed main categories") != std::string::npos);
    CHECK(captured_context->find("The main category is already fixed to:") == std::string::npos);
    CHECK(captured_context->find("Return only the subcategory text on a single line.") == std::string::npos);
}

TEST_CASE("CategorizationService normalizes supported image main categories to Images") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    struct TestCase {
        const char* file_name;
        const char* description;
        const char* response;
        const char* expected_subcategory;
    };

    const std::vector<TestCase> cases = {
        {"screensehot1.png",
         "A screenshot of LLM mode options for a file sorter application.",
         "LLM Mode Options : File Sorters",
         "File Sorters"},
        {"must-take-pictures-in-paris-facebook.jpg",
         "A travel photo highlighting the Eiffel Tower and the Paris skyline.",
         "Paris_Skyline : Eiffel_Tower",
         "Eiffel_Tower"},
        {"lion-805084_1920.jpg",
         "A lion standing in tall grass on an African savanna.",
         "African Fauna : Lions",
         "Lions"},
        {"image5.jpg",
         nullptr,
         "Grass : Flowers",
         "Flowers"},
        {"GettyImages-865719-001.jpg",
         nullptr,
         "Wildlife : Sea Lions",
         "Sea Lions"}
    };

    for (const auto& test_case : cases) {
        TempDir data_dir;
        const std::string full_path = (data_dir.path() / test_case.file_name).string();
        const std::string prompt_path = test_case.description
            ? MainAppTestAccess::build_image_prompt_path(
                  full_path,
                  test_case.file_name,
                  test_case.description)
            : full_path;
        const std::vector<FileEntry> files = {FileEntry{full_path, test_case.file_name, FileType::File}};

        std::atomic<bool> stop_flag{false};
        auto calls = std::make_shared<int>(0);
        auto factory = [calls, &test_case]() {
            return std::make_unique<FixedResponseLLM>(calls, test_case.response);
        };

        const auto categorized = service.categorize_entries(
            files,
            true,
            stop_flag,
            {},
            {},
            {},
            {},
            factory,
            [&test_case, &prompt_path](const FileEntry&) {
                return CategorizationService::PromptOverride{test_case.file_name, prompt_path};
            });

        CAPTURE(test_case.file_name);
        REQUIRE(categorized.size() == 1);
        CHECK(categorized.front().canonical_category == "Images");
        CHECK(categorized.front().canonical_subcategory == test_case.expected_subcategory);
    }
}

TEST_CASE("CategorizationService preserves explicit whitelist image main categories when Images is disallowed") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Animals", "Travel"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "cat_photo.jpg";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string prompt_path = MainAppTestAccess::build_image_prompt_path(
        full_path,
        file_name,
        "A grey cat sleeping on a sofa.");
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Animals : Cats");
    };

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        {},
        factory,
        [file_name, prompt_path](const FileEntry&) {
            return CategorizationService::PromptOverride{file_name, prompt_path};
        });

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().canonical_category == "Animals");
    CHECK(categorized.front().canonical_subcategory == "Cats");
}

TEST_CASE("CategorizationService adds subject-focused guidance for screenshot-like image prompts") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string prompt_name = "dashboard_interface.png";
    const std::string prompt_path = MainAppTestAccess::build_image_prompt_path(
        (base_dir.path() / prompt_name).string(),
        prompt_name,
        "A screenshot of a dashboard interface with charts, navigation, and visible labels.");

    const std::string context = CategorizationServiceTestAccess::build_combined_context(
        service,
        {},
        prompt_name,
        prompt_path,
        FileType::File);

    CHECK(context.find("Image categorization guidance:") != std::string::npos);
    CHECK(context.find("Always use Images as the main category") != std::string::npos);
    CHECK(context.find("Categorize the subject matter shown in the image") != std::string::npos);
    CHECK(context.find("Do not classify a PNG/JPG/WebP screenshot as Software") != std::string::npos);
    CHECK(context.find("prefer labels that describe what is on screen") != std::string::npos);
}

TEST_CASE("CategorizationService skips extension-only consistency hints for rich image prompts") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_use_consistency_hints(true);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    const auto existing = db.resolve_category("Operating Systems", "Dashboards");
    REQUIRE(existing.taxonomy_id > 0);
    REQUIRE(db.insert_or_update_file_with_categorization(
        "older_dashboard.png", "F", dir_path, existing, true, std::string(), false));

    const std::string file_name = "screenshot-33.png";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::string prompt_name = "computer_interface_dark.png";
    const std::string prompt_path = MainAppTestAccess::build_image_prompt_path(
        full_path,
        prompt_name,
        "A screenshot of a dark computer interface with panels and navigation.");
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto captured_path = std::make_shared<std::string>();
    auto captured_context = std::make_shared<std::string>();
    auto factory = [captured_path, captured_context, calls]() {
        return std::make_unique<PromptCaptureLLM>(
            captured_path,
            captured_context,
            calls,
            "Category: Images\nSubcategory: Screenshots");
    };

    const auto categorized = service.categorize_entries(
        files,
        true,
        stop_flag,
        {},
        {},
        {},
        {},
        factory,
        [prompt_name, prompt_path](const FileEntry&) {
            return CategorizationService::PromptOverride{prompt_name, prompt_path};
        });

    REQUIRE(categorized.size() == 1);
    CHECK(*calls == 1);
    CHECK(captured_path->find(prompt_name) != std::string::npos);
    CHECK(captured_context->find("Image categorization guidance:") != std::string::npos);
    CHECK(captured_context->find("Recent assignments for similar items:") == std::string::npos);
}

TEST_CASE("CategorizationService stores canonical English labels and persists translated taxonomy labels") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_category_language(CategoryLanguage::French);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "Git-2.50.0.2-64-bit.exe";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto categorize_calls = std::make_shared<int>(0);
    auto translation_calls = std::make_shared<int>(0);
    auto factory = [categorize_calls, translation_calls]() {
        return std::make_unique<TranslationAwareLLM>(
            categorize_calls,
            translation_calls,
            "Software : Version Control",
            std::deque<std::string>{
                "{\"category\":\"Logiciels\",\"subcategory\":\"Controle de version\"}"
            });
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
    CHECK(categorized.front().category == "Logiciels");
    CHECK(categorized.front().subcategory == "Controle de version");
    CHECK(categorized.front().canonical_category == "Software");
    CHECK(categorized.front().canonical_subcategory == "Version Control");
    CHECK(*categorize_calls == 1);
    CHECK(*translation_calls == 1);

    const auto cached = db.get_categorization_from_db(data_dir.path().string(), file_name, FileType::File);
    REQUIRE(cached.size() == 2);
    CHECK(cached[0] == "Software");
    CHECK(cached[1] == "Version Control");

    const auto translated = db.get_category_translation(categorized.front().taxonomy_id, CategoryLanguage::French);
    REQUIRE(translated.has_value());
    CHECK(translated->category == "Logiciels");
    CHECK(translated->subcategory == "Controle de version");

    const auto resolved_french = db.resolve_category_for_language("Logiciels", "Controle de version", CategoryLanguage::French);
    CHECK(resolved_french.taxonomy_id == categorized.front().taxonomy_id);
    CHECK(resolved_french.category == "Software");
    CHECK(resolved_french.subcategory == "Version Control");

    const auto cached_entries = service.load_cached_entries(data_dir.path().string());
    REQUIRE(cached_entries.size() == 1);
    CHECK(cached_entries.front().category == "Logiciels");
    CHECK(cached_entries.front().subcategory == "Controle de version");
    CHECK(cached_entries.front().canonical_category == "Software");
    CHECK(cached_entries.front().canonical_subcategory == "Version Control");
}

TEST_CASE("CategorizationService strips inline subcategory artifacts from translated category labels") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_category_language(CategoryLanguage::French);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "francais_notes.txt";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto categorize_calls = std::make_shared<int>(0);
    auto translation_calls = std::make_shared<int>(0);
    auto factory = [categorize_calls, translation_calls]() {
        return std::make_unique<TranslationAwareLLM>(
            categorize_calls,
            translation_calls,
            "Documents : French Language Notes",
            std::deque<std::string>{
                "{\"category\":\"Documents , subcategory Notes en Francais\","
                "\"subcategory\":\"Notes en Francais\"}"
            });
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
    CHECK(categorized.front().subcategory == "Notes en Francais");
    CHECK(categorized.front().canonical_category == "Documents");
    CHECK(categorized.front().canonical_subcategory == "French Language Notes");
    CHECK(*categorize_calls == 1);
    CHECK(*translation_calls == 1);

    const auto translated = db.get_category_translation(categorized.front().taxonomy_id, CategoryLanguage::French);
    REQUIRE(translated.has_value());
    CHECK(translated->category == "Documents");
    CHECK(translated->subcategory == "Notes en Francais");
}

TEST_CASE("CategorizationService strips inline subcategory label artifacts when parsing service output") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string file_name = "seal_photo.jpg";
    const std::string full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<FixedResponseLLM>(calls, "Category: Images, subcategory: Funny seals");
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
    CHECK(categorized.front().subcategory == "Funny seals");
    CHECK(*calls == 1);
}
