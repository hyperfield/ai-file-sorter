#include "AppTestRunner.hpp"

#include "CategorizationService.hpp"
#include "DatabaseManager.hpp"
#include "ILLMClient.hpp"
#include "Settings.hpp"
#include "UserLearningStore.hpp"
#include "WhitelistTestFixtures.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace {

class EnvironmentOverride {
public:
    EnvironmentOverride(std::string key, std::string value)
        : key_(std::move(key))
    {
        if (const char* current = std::getenv(key_.c_str())) {
            previous_ = std::string(current);
        }
        set(value);
    }

    ~EnvironmentOverride()
    {
        if (previous_) {
            set(*previous_);
        } else {
            unset();
        }
    }

    EnvironmentOverride(const EnvironmentOverride&) = delete;
    EnvironmentOverride& operator=(const EnvironmentOverride&) = delete;

private:
    void set(const std::string& value) const
    {
#ifdef _WIN32
        _putenv_s(key_.c_str(), value.c_str());
#else
        setenv(key_.c_str(), value.c_str(), 1);
#endif
    }

    void unset() const
    {
#ifdef _WIN32
        _putenv_s(key_.c_str(), "");
#else
        unsetenv(key_.c_str());
#endif
    }

    std::string key_;
    std::optional<std::string> previous_;
};

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto base = std::filesystem::temp_directory_path();
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = base / ("aifs-self-test-" + std::to_string(nonce) + "-" +
                                     std::to_string(attempt));
            std::error_code ec;
            if (std::filesystem::create_directories(candidate, ec)) {
                path_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("Unable to create temporary self-test directory.");
    }

    ~TemporaryDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

class CapturingLLM final : public ILLMClient {
public:
    explicit CapturingLLM(std::shared_ptr<std::string> captured_context,
                          std::string response)
        : captured_context_(std::move(captured_context)),
          response_(std::move(response))
    {
    }

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string& consistency_context) override
    {
        if (captured_context_) {
            *captured_context_ = consistency_context;
        }
        return response_;
    }

    std::string complete_prompt(const std::string&, int) override
    {
        return {};
    }

    void set_prompt_logging_enabled(bool) override
    {
    }

private:
    std::shared_ptr<std::string> captured_context_;
    std::string response_;
};

struct SelfTestContext {
    TemporaryDirectory temp_dir;
    EnvironmentOverride config_dir_override;
    Settings settings;
    DatabaseManager db;

    SelfTestContext()
        : temp_dir(),
          config_dir_override("AI_FILE_SORTER_CONFIG_DIR", temp_dir.path().string()),
          settings(),
          db(settings.get_config_dir())
    {
    }
};

std::string normalize_suite(std::string suite)
{
    std::transform(suite.begin(), suite.end(), suite.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (suite.empty()) {
        return "all";
    }
    if (suite == "whitelists") {
        return "whitelist";
    }
    return suite;
}

bool contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

AppTestRunner::CaseResult make_case_result(std::string name,
                                           bool passed,
                                           std::string message)
{
    return AppTestRunner::CaseResult{std::move(name), passed, std::move(message)};
}

std::vector<CategorizedFile> categorize_one(CategorizationService& service,
                                            const std::filesystem::path& base,
                                            const std::string& file_name,
                                            const std::string& llm_response,
                                            std::shared_ptr<std::string> captured_context)
{
    const std::vector<FileEntry> entries = {
        FileEntry{(base / file_name).string(), file_name, FileType::File}
    };
    std::atomic<bool> stop_flag{false};
    auto factory = [captured_context, llm_response]() {
        return std::make_unique<CapturingLLM>(captured_context, llm_response);
    };
    return service.categorize_entries(entries, true, stop_flag, {}, {}, {}, {}, factory);
}

AppTestRunner::CaseResult test_large_whitelist_uses_compact_prompt()
{
    SelfTestContext context;
    const auto preset = WhitelistTestFixtures::large_whitelist_preset();
    context.settings.set_use_whitelist(true);
    context.settings.set_allowed_categories(preset.categories);
    context.settings.set_allowed_subcategories(preset.subcategories);
    context.settings.set_allowed_subcategories_by_category(preset.subcategories_by_category);

    CategorizationService service(context.settings, context.db, nullptr);
    auto captured_context = std::make_shared<std::string>();
    const auto categorized = categorize_one(service,
                                           context.temp_dir.path(),
                                           "camera_manual.pdf",
                                           "Category: Manuals\nSubcategory: Camera Guides",
                                           captured_context);

    const bool passed = categorized.size() == 1 &&
                        contains(*captured_context, "Selected whitelist is large") &&
                        contains(*captured_context, "Manuals") &&
                        !contains(*captured_context, "Archive Bucket 79") &&
                        !contains(*captured_context, "Allowed main categories");
    if (passed) {
        return make_case_result("large whitelist uses compact prompt candidates",
                                true,
                                "Prompt was reduced to relevant candidates.");
    }
    return make_case_result("large whitelist uses compact prompt candidates",
                            false,
                            "Expected compact candidate block with Manuals and no full whitelist dump.");
}

AppTestRunner::CaseResult test_large_whitelist_prefers_learned_candidate()
{
    SelfTestContext context;
    const auto preset = WhitelistTestFixtures::large_whitelist_preset();
    context.settings.set_use_whitelist(true);
    context.settings.set_allowed_categories(preset.categories);
    context.settings.set_allowed_subcategories({});
    context.settings.set_allowed_subcategories_by_category({});

    UserLearningStore learning_store(context.settings.get_config_dir());
    std::string error;
    UserLearningStore::ApprovedMapping mapping;
    mapping.file_name = "nikon_camera_manual.pdf";
    mapping.file_type = FileType::File;
    mapping.dir_path = (context.temp_dir.path() / "approved").string();
    mapping.category = "Manuals";
    mapping.subcategory = "Camera Guides";
    mapping.context_text = "Camera aperture setup and lens maintenance instructions.";
    if (!learning_store.record_approved_mapping(mapping, &error)) {
        return make_case_result("large whitelist prefers learned candidate",
                                false,
                                "Could not seed learning store: " + error);
    }

    CategorizationService service(context.settings, context.db, nullptr, &learning_store);
    auto captured_context = std::make_shared<std::string>();
    const auto categorized = categorize_one(service,
                                           context.temp_dir.path(),
                                           "camera_setup_manual.pdf",
                                           "Category: Documents\nSubcategory: General",
                                           captured_context);

    const bool passed = categorized.size() == 1 &&
                        categorized.front().canonical_category == "Manuals" &&
                        categorized.front().canonical_subcategory == "Camera Guides" &&
                        contains(*captured_context, "Manuals : Camera Guides") &&
                        !contains(*captured_context, "User-learned category candidates");
    if (passed) {
        return make_case_result("large whitelist prefers learned candidate",
                                true,
                                "Review-confirmed Manuals candidate outranked generic Documents output.");
    }
    return make_case_result("large whitelist prefers learned candidate",
                            false,
                            "Expected Manuals : Camera Guides from learned behavior.");
}

AppTestRunner::CaseResult test_large_whitelist_preserves_unicode_labels()
{
    SelfTestContext context;
    const auto preset = WhitelistTestFixtures::large_whitelist_preset();
    context.settings.set_use_whitelist(true);
    context.settings.set_allowed_categories(preset.categories);
    context.settings.set_allowed_subcategories(preset.subcategories);
    context.settings.set_allowed_subcategories_by_category(preset.subcategories_by_category);

    CategorizationService service(context.settings, context.db, nullptr);
    auto captured_context = std::make_shared<std::string>();
    const auto categorized = categorize_one(service,
                                           context.temp_dir.path(),
                                           "ab_testing_cloud_notes.pdf",
                                           "Category: AB Testing ☁️\nSubcategory: Experiment Notes",
                                           captured_context);

    const bool passed = categorized.size() == 1 &&
                        categorized.front().canonical_category == "AB Testing ☁️" &&
                        contains(*captured_context, "AB Testing ☁️");
    if (passed) {
        return make_case_result("large whitelist preserves unicode labels",
                                true,
                                "Unicode whitelist labels remained available to categorization.");
    }
    return make_case_result("large whitelist preserves unicode labels",
                            false,
                            "Expected AB Testing ☁️ to survive prompt selection and parsing.");
}

std::vector<AppTestRunner::CaseResult> run_whitelist_suite()
{
    std::vector<AppTestRunner::CaseResult> results;
    results.push_back(test_large_whitelist_uses_compact_prompt());
    results.push_back(test_large_whitelist_prefers_learned_candidate());
    results.push_back(test_large_whitelist_preserves_unicode_labels());
    return results;
}

} // namespace

bool AppTestRunner::Result::passed() const
{
    if (!error.empty()) {
        return false;
    }
    return std::all_of(cases.begin(), cases.end(), [](const CaseResult& result) {
        return result.passed;
    });
}

AppTestRunner::Result AppTestRunner::run(const Options& options) const
{
    Result result;
    result.suite = normalize_suite(options.suite);

    if (result.suite != "all" && result.suite != "whitelist") {
        result.error = "Unknown test suite '" + options.suite + "'. Supported suites: all, whitelist.";
        return result;
    }

    if (result.suite == "all" || result.suite == "whitelist") {
        const auto whitelist_results = run_whitelist_suite();
        result.cases.insert(result.cases.end(), whitelist_results.begin(), whitelist_results.end());
    }

    return result;
}

std::vector<std::string> AppTestRunner::available_suites()
{
    return {"all", "whitelist"};
}
