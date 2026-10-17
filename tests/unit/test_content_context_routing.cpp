#include "AnalysisCoordinator.hpp"
#include "CategorizationService.hpp"
#include "DatabaseManager.hpp"
#include "FolderStructurePattern.hpp"
#include "FolderTreeRoutingPlan.hpp"
#include "ILLMClient.hpp"
#include "LocalFsProvider.hpp"
#include "ResultsCoordinator.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

#include <atomic>
#include <QImage>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace {
struct Request {
    std::string name;
    std::string path;
    std::string context;
};

class RoutingLLM : public ILLMClient {
public:
    RoutingLLM(std::shared_ptr<std::vector<Request>> requests,
               std::function<std::string(const Request&)> respond)
        : requests_(std::move(requests)), respond_(std::move(respond)) {}

    std::string categorize_file(const std::string& name, const std::string& path,
                                FileType, const std::string& context) override
    {
        requests_->push_back({name, path, context});
        return respond_(requests_->back());
    }
    std::string complete_prompt(const std::string&, int) override { return {}; }
    void set_prompt_logging_enabled(bool) override {}

private:
    std::shared_ptr<std::vector<Request>> requests_;
    std::function<std::string(const Request&)> respond_;
};

struct WorkflowResult {
    AnalysisRunResult result;
    std::vector<CategorizedFile> review;
};

WorkflowResult run_cached_image_workflow(Settings& settings, DatabaseManager& database,
    const std::filesystem::path& source, const std::shared_ptr<std::vector<Request>>& requests)
{
    LocalFsProvider provider;
    ResultsCoordinator results(provider);
    CategorizationService service(settings, database, nullptr);
    bool local = true;
    std::vector<CategorizedFile> cached, categorized, review;
    std::vector<FileEntry> pending;
    std::atomic<bool> stop{false};
    std::optional<bool> fallback;
    auto logger = std::make_shared<spdlog::logger>("content-workflow-test",
        std::make_shared<spdlog::sinks::null_sink_mt>());
    AnalysisWorkflowContext context{settings, database, service, results, logger, local,
                                    cached, categorized, pending, review, stop, fallback};
    const auto ignore = [](auto&&...) {};
    context.get_folder_path = [&] { return source.string(); };
    context.tr = [](const char* text) { return QString::fromUtf8(text); };
    context.should_abort_analysis = [] { return false; };
    context.append_progress = ignore;
    context.prune_empty_cached_entries_for = [&](const std::string& path) { database.remove_empty_categorizations(path); };
    context.log_cached_highlights = ignore;
    context.log_pending_queue = ignore;
    context.effective_scan_options = [] { return FileScanOptions::Files; };
    context.filter_file_entries = ignore;
    context.notify_review_preview_changed = ignore;
    context.suggest_johnny_decimal_plugin = [] { return false; };
    context.configure_progress_stages = ignore;
    context.set_progress_stage_items = ignore;
    context.set_progress_active_stage = ignore;
    context.mark_progress_stage_item_in_progress = ignore;
    context.mark_progress_stage_item_completed = ignore;
    context.mark_progress_stage_item_skipped = ignore;
    context.should_log_prompts = [] { return false; };
    context.localize_suggested_names = false;
    context.prompt_visual_cpu_fallback = [](const std::string&) { return false; };
    context.prompt_continue_without_visual_analysis = [](const std::string&) { return false; };
    context.notify_recategorization_reset = ignore;
    context.make_llm_client = [requests] {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            return request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos
                ? std::string("Legal : Data Protection")
                : std::string(R"({"targetFolder":"Legal/Data Protection","createFolder":false})");
        });
    };
    const auto result = AnalysisCoordinator(std::move(context)).execute();
    return {result, std::move(review)};
}
} // namespace

TEST_CASE("Content evidence reaches classification and routing independently of renaming", "[content-routing]")
{
    const bool allow_new = GENERATE(false, true);
    const bool image = GENERATE(false, true);
    const bool broad_category = GENERATE(false, true);
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_offer_rename_documents(false);
    settings.set_offer_rename_images(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(allow_new);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "80-89 Legal" / "82 Data Protection");
    std::filesystem::create_directories(archive.path() / "50-59 Engineering" / "51 Roadmaps");
    if (broad_category) {
        std::filesystem::create_directories(archive.path() / "Documents" / "Policies");
    }
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    const auto name = image ? "IMG_0001.png" : "Document1.pdf";
    const auto suggested = image ? "policy_changes.png" : "policy_changes.pdf";
    const auto path = source.path() / name;
    std::ofstream(path) << "Privacy policy and customer data deletion requirements";
    const auto key = ContentAnalysisCache::make_key(path, image ? "image:test-v1" : "document:test-v1");
    REQUIRE(key);
    REQUIRE(database.content_analysis_cache().store(*key, {suggested, "Customer data protection and privacy obligations."}));
    const auto evidence = database.content_analysis_cache().find(*key);
    REQUIRE(evidence);
    const auto prompt_path = image
        ? AnalysisCoordinator::build_image_prompt_path(path.string(), evidence->suggested_name, evidence->description)
        : AnalysisCoordinator::build_document_prompt_path(path.string(), evidence->suggested_name, evidence->description);
    auto requests = std::make_shared<std::vector<Request>>();
    auto factory = [requests, allow_new, broad_category]() {
        return std::make_unique<RoutingLLM>(requests, [allow_new, broad_category](const Request& request) {
            if (request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos) {
                return broad_category ? std::string("Documents : Policies") : std::string("Legal : Data Protection");
            }
            if (broad_category) {
                return std::string(R"({"targetFolder":"80-89 Legal/82 Data Protection","createFolder":false})");
            }
            return allow_new
                ? std::string(R"({"targetFolder":"100-109 Policies/101 Changes","createFolder":true})")
                : std::string(R"({"targetFolder":"50-59 Engineering/51 Roadmaps","createFolder":false})");
        });
    };
    CategorizationService service(settings, database, nullptr);
    std::atomic<bool> stop{false};
    const auto result = service.categorize_entries(
        {{path.string(), name, FileType::File}}, true, stop, {}, {}, {}, {}, factory,
        [&](const FileEntry&) { return CategorizationService::PromptOverride{suggested, prompt_path}; });
    REQUIRE(result.size() == 1);
    CHECK(result.front().target_folder_relative_path == "80-89 Legal/82 Data Protection");
    CHECK(result.front().target_folder_exists);
    CHECK_FALSE(result.front().target_folder_suggested_new);
    CHECK(result.front().suggested_name.empty());
    REQUIRE(requests->size() == 2);
    for (const auto& request : *requests) {
        CHECK(request.context.find(evidence->description) != std::string::npos);
        CHECK(request.context.find("Content evidence (primary") != std::string::npos);
        CHECK(request.path.find('\n') == std::string::npos);
    }
}

TEST_CASE("Review plan reserves new numbers across batches and reuses proposed destinations", "[content-routing]")
{
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(true);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "10-19 Finance" / "11 Invoices");
    std::filesystem::create_directories(archive.path() / "10-19 Finance" / "12 Banking");
    std::filesystem::create_directories(archive.path() / "20-29 Work" / "21 Projects");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    FolderTreeRoutingPlan plan;
    auto requests = std::make_shared<std::vector<Request>>();
    auto factory = [requests]() {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            if (request.context.find(FolderTreeCatalog::kPromptMarker) != std::string::npos) {
                return std::string(R"({"targetFolder":"10-19 Finance/15 Suggested","createFolder":true})");
            }
            return request.name.find("forecast") != std::string::npos
                ? std::string("Finance : Forecasts") : std::string("Finance : Budgets");
        });
    };
    std::vector<std::string> targets;
    std::atomic<bool> stop{false};
    for (const auto* name : {"budget.pdf", "forecast.pdf", "budget_update.pdf"}) {
        const auto result = service.categorize_entries(
            {{(source.path() / name).string(), name, FileType::File}},
            true, stop, {}, {}, {}, {}, factory, {}, {}, {}, &plan);
        REQUIRE(result.size() == 1);
        CHECK(result.front().target_folder_suggested_new);
        CHECK_FALSE(result.front().target_folder_exists);
        targets.push_back(result.front().target_folder_relative_path);
    }
    CHECK(targets == std::vector<std::string>{"10-19 Finance/13 Budgets", "10-19 Finance/14 Forecasts",
                                            "10-19 Finance/13 Budgets"});
    CHECK_FALSE(std::filesystem::exists(archive.path() / "10-19 Finance" / "13 Budgets"));
    CHECK_FALSE(plan.reserve(archive.path().string(), "../outside"));
    CHECK(plan.planning_catalog("another-root", FolderTreeCatalog::Catalog{}).empty());
}

TEST_CASE("AnalysisCoordinator reuses complete image evidence without a visual backend", "[content-routing]")
{
    const bool complete = GENERATE(false, true);
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_analyze_images_by_content(true);
    settings.set_analyze_documents_by_content(false);
    settings.set_offer_rename_images(false);
    settings.set_use_consistency_hints(false);
    settings.set_visual_model_id("custom:missing-cache-test-model");
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(false);
    TempDir source;
    TempDir archive;
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    std::filesystem::create_directories(archive.path() / "Legal" / "Data Protection");
    const auto image_path = source.path() / "IMG_0001.png";
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::white);
    REQUIRE(image.save(QString::fromStdString(image_path.string())));
    DatabaseManager database(settings.get_config_dir());
    REQUIRE(database.insert_or_update_file_with_categorization(
        "IMG_0001.png", "F", source.path().string(), {0, "", ""}, false, "policy_changes.png"));
    const auto key = ContentAnalysisCache::make_key(image_path, "image-v1:" + settings.get_visual_model_id());
    REQUIRE(key);
    if (complete) {
        REQUIRE(database.content_analysis_cache().store(
            *key, {"policy_changes.png", "Customer data protection and privacy obligations."}));
    }
    const auto requests = std::make_shared<std::vector<Request>>();
    const auto run = run_cached_image_workflow(settings, database, source.path(), requests);
    INFO(run.result.error_message);
    if (!complete) {
        CHECK(run.result.status == AnalysisRunStatus::Failed);
        CHECK(requests->empty());
        CHECK_FALSE(database.get_categorized_file(source.path().string(), "IMG_0001.png", FileType::File));
        return;
    }
    REQUIRE(run.result.status == AnalysisRunStatus::Completed);
    REQUIRE(run.review.size() == 1);
    CHECK(run.review.front().target_folder_relative_path == "Legal/Data Protection");
    REQUIRE(requests->size() == 2);
    for (const auto& request : *requests) {
        CHECK(request.context.find("Customer data protection and privacy obligations.") != std::string::npos);
    }
    const auto resumed = run_cached_image_workflow(settings, database, source.path(), requests);
    REQUIRE(resumed.result.status == AnalysisRunStatus::Completed);
    REQUIRE(resumed.review.size() == 1);
    CHECK(resumed.review.front().learning_context.find("privacy obligations") != std::string::npos);
    CHECK(requests->size() == 2);
    image.fill(Qt::black);
    REQUIRE(image.save(QString::fromStdString(image_path.string())));
    const auto changed = run_cached_image_workflow(settings, database, source.path(), requests);
    CHECK(changed.result.status == AnalysisRunStatus::Failed);
    CHECK(requests->size() == 2);
    CHECK(std::filesystem::exists(image_path));
}

TEST_CASE("Numbered folder allocation stays within exhausted ranges", "[content-routing]")
{
    std::vector<FolderTreeCatalog::Entry> entries;
    for (int start = 10; start <= 90; start += 10) {
        entries.push_back({std::to_string(start) + "-" + std::to_string(start + 9) + " Area", 1});
    }
    entries.push_back({"10-19 Area/11 Existing", 2});
    entries.push_back({"30-39 Area/31 Existing", 2});
    CHECK(FolderStructurePattern::infer_profile(FolderTreeCatalog::Catalog(entries)).has_johnny_decimal_like_ranges);
    CHECK_FALSE(FolderStructurePattern::suggest_new_folder(FolderTreeCatalog::Catalog(entries), "Music", "Albums"));
    entries.erase(entries.begin() + 1);
    const auto gap = FolderStructurePattern::suggest_new_folder(FolderTreeCatalog::Catalog(entries), "Music", "Albums");
    REQUIRE(gap);
    CHECK(gap->relative_path == "20-29 Music/21 Albums");
    entries = {{"10-19 Finance", 1}, {"20-29 Work", 1}};
    for (int number = 11; number <= 19; ++number) {
        entries.push_back({"10-19 Finance/" + std::to_string(number) + " Existing", 2});
    }
    CHECK_FALSE(FolderStructurePattern::suggest_new_folder(FolderTreeCatalog::Catalog(entries), "Finance", "Budgets"));
}
