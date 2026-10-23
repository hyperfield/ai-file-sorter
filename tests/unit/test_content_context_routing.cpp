#include "AnalysisCoordinator.hpp"
#include "CategorizationService.hpp"
#include "DatabaseManager.hpp"
#include "FolderStructurePattern.hpp"
#include "FolderTreeRoutingPlan.hpp"
#include "FolderTreeSelectionContract.hpp"
#include "FolderPurposeMetadata.hpp"
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
#include <sqlite3.h>
#include <vector>

namespace {
struct Request {
    std::string name;
    std::string path;
    std::string context;
    std::string schema;
};

class RoutingLLM : public ILLMClient {
public:
    RoutingLLM(std::shared_ptr<std::vector<Request>> requests,
               std::function<std::string(const Request&)> respond)
        : requests_(std::move(requests)), respond_(std::move(respond)) {}

    std::string categorize_file(const std::string& name, const std::string& path,
                                FileType, const std::string& context) override
    {
        requests_->push_back({name, path, context, {}});
        return respond_(requests_->back());
    }
    std::string categorize_file_constrained(const std::string& name, const std::string& path,
        FileType, const std::string& context, const LLMResponseConstraint& constraint) override
    {
        requests_->push_back({name, path, context, constraint.json_schema});
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
    auto factory = [requests, broad_category]() {
        return std::make_unique<RoutingLLM>(requests, [broad_category](const Request& request) {
            if (request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos) {
                return broad_category ? std::string("Documents : Policies") : std::string("Legal : Data Protection");
            }
            return std::string(R"({"targetFolder":"80-89 Legal/82 Data Protection","createFolder":false})");
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
    CHECK(requests->front().schema.empty());
    CHECK_FALSE(requests->back().schema.empty());
    CHECK(requests->back().schema.find("80-89 Legal/82 Data Protection") != std::string::npos);
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
                if (request.context.find(FolderTreeSelectionContract::kProposalMarker) == std::string::npos) {
                    if (request.name.find("budget") != std::string::npos &&
                        request.context.find("10-19 Finance/13 Budgets\n") != std::string::npos) {
                        return std::string(R"({"targetFolder":"10-19 Finance/13 Budgets","createFolder":false})");
                    }
                    return std::string(R"({"targetFolder":null,"createFolder":false})");
                }
                return request.name.find("forecast") != std::string::npos
                    ? std::string(R"({"targetFolder":"10-19 Finance/13 Forecasts","createFolder":true})")
                    : std::string(R"({"targetFolder":"10-19 Finance/13 Budgets","createFolder":true})");
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

TEST_CASE("Review plan matches pending labels only within the same literal parent", "[content-routing][folder-plan]")
{
    FolderTreeRoutingPlan plan;
    const std::string root = "archive";
    const std::string first = "70-79 Media & Design/75 Landscape";
    REQUIRE(plan.reserve(root, first));
    CHECK(plan.find_equivalent_proposal(root, "70-79 Media & Design/76   LANDSCAPE", true) == first);
    CHECK(plan.find_proposed_child(root, "70-79 Media & Design", "landscape", true) == first);
    CHECK_FALSE(plan.find_equivalent_proposal("another-archive", first, true));
    CHECK_FALSE(plan.find_equivalent_proposal(root, "60-69 Research/65 Landscape", true));
    CHECK_FALSE(plan.find_equivalent_proposal(root, "70-79 Media & Design/76 Landscape Paintings", true));
    CHECK_FALSE(plan.find_equivalent_proposal(root, "../75 Landscape", true));
    CHECK_FALSE(plan.find_proposed_child(root, "70-79 Media & Design", "../Landscape", true));
    CHECK_FALSE(plan.find_proposed_child(root, "70-79 Media & Design", "", true));

    REQUIRE(plan.reserve(root, "Projects/21 Alpha/21.01 Notes"));
    CHECK(plan.find_equivalent_proposal(root, "Projects/21 Alpha/21.02 Notes", true) ==
          "Projects/21 Alpha/21.01 Notes");
    CHECK_FALSE(plan.find_equivalent_proposal(root, "Projects/22 Alpha/22.02 Notes", true));
    REQUIRE(plan.reserve(root, "Media/75 Caf\xc3\xa9"));
    CHECK(plan.find_proposed_child(root, "Media", "cafe\xcc\x81", true) == "Media/75 Caf\xc3\xa9");
}

TEST_CASE("Content evidence and response constraints survive abstention and proposal retries", "[content-routing]")
{
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(true);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Finance/Payroll");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    auto requests = std::make_shared<std::vector<Request>>();
    const auto factory = [requests] {
        return std::make_unique<RoutingLLM>(requests, [attempt = 0](const Request& request) mutable {
            if (request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos) {
                return std::string("Photography : Landscapes");
            }
            if (request.context.find(FolderTreeSelectionContract::kProposalMarker) == std::string::npos) {
                return std::string(R"({"targetFolder":null,"createFolder":false})");
            }
            return ++attempt == 1 ? std::string(R"({"targetFolder":"../outside","createFolder":true})")
                                  : std::string(R"({"targetFolder":"Photography/Landscapes","createFolder":true})");
        });
    };
    const std::string evidence = "Mountain landscape photography collection.\nCopyright notes: https://example.org/photos?q=mountain";
    const auto path = (source.path() / "unhelpful-name.pdf").string();
    const auto prompt_path = AnalysisCoordinator::build_document_prompt_path(path, "mountain_landscape.pdf", evidence);
    std::atomic<bool> stop{false};
    const auto result = service.categorize_entries({{path, "unhelpful-name.pdf", FileType::File}},
        true, stop, {}, {}, {}, {}, factory,
        [&](const FileEntry&) { return CategorizationService::PromptOverride{"mountain_landscape.pdf", prompt_path}; });
    REQUIRE(result.size() == 1);
    CHECK(result.front().target_folder_relative_path == "Photography/Landscapes");
    CHECK(result.front().target_folder_suggested_new);
    CHECK_FALSE(std::filesystem::exists(archive.path() / "Photography"));
    REQUIRE(requests->size() == 4);
    for (std::size_t i = 0; i < requests->size(); ++i) {
        const auto& request = requests->at(i);
        CHECK(request.context.find(evidence) != std::string::npos);
        CHECK(request.path.find('\n') == std::string::npos);
        CHECK(request.schema.empty() == (i == 0));
        CHECK((request.context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos) == (i >= 2));
    }
    CHECK(requests->at(1).schema.find("enum") != std::string::npos);
    CHECK(requests->at(2).schema == requests->at(3).schema);
    CHECK(requests->at(3).context.find("Invalid proposal") != std::string::npos);
}

TEST_CASE("Review plan preserves meaningful numbers in literal and human labels", "[content-routing][folder-plan]")
{
    FolderTreeRoutingPlan plan;
    REQUIRE(plan.reserve("archive", "Reports/2024 Taxes"));
    CHECK(plan.find_equivalent_proposal("archive", "Reports/2024 taxes", false) == "Reports/2024 Taxes");
    CHECK_FALSE(plan.find_equivalent_proposal("archive", "Reports/2025 Taxes", false));
    REQUIRE(plan.reserve("archive", "Projects/75"));
    CHECK_FALSE(plan.find_equivalent_proposal("archive", "Projects/76", true));
    REQUIRE(plan.reserve("archive", "Finance/15 2024 Taxes"));
    CHECK(plan.find_proposed_child("archive", "Finance", "2024 Taxes", true) == "Finance/15 2024 Taxes");
    CHECK_FALSE(plan.find_proposed_child("archive", "Finance", "2025 Taxes", true));
}

TEST_CASE("CategorizationService reuses weak Landscape proposals even when the range fills", "[content-routing][folder-plan]")
{
    const bool cached_routes = GENERATE(false, true);
    const bool last_slot = GENERATE(false, true);
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(true);
    TempDir source;
    TempDir archive;
    const std::string parent = "70-79 Media & Design";
    const int first_available = last_slot ? 79 : 75;
    for (int number = 71; number < first_available; ++number) {
        std::filesystem::create_directories(archive.path() / parent / (std::to_string(number) + " Existing"));
    }
    std::filesystem::create_directories(archive.path() / "60-69 Research & Knowledge" / "64 Notes");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    const auto catalog = FolderTreeCatalog::Catalog::scan(archive.path());
    const auto expected = parent + "/" + std::to_string(first_available) + " Landscape";
    CHECK_FALSE(FolderTreeCatalog::is_strong_semantic_match(
        FolderTreeCatalog::semantic_match_score(expected, "Images", "Landscape")));
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    FolderTreeRoutingPlan plan;
    const auto requests = std::make_shared<std::vector<Request>>();
    const auto factory = [requests, parent, expected, &settings] {
        return std::make_unique<RoutingLLM>(requests, [parent, expected, &settings](const Request& request) {
            if (!settings.get_suggest_new_folders() &&
                request.context.find(FolderTreeCatalog::kPromptMarker) != std::string::npos) {
                return "{\"targetFolder\":\"" + parent + "\",\"createFolder\":false}";
            }
            if (request.context.find(FolderTreeCatalog::kPromptMarker) != std::string::npos &&
                request.context.find(FolderTreeSelectionContract::kProposalMarker) == std::string::npos) {
                if (request.context.find(expected + "\n") != std::string::npos) {
                    return "{\"targetFolder\":\"" + expected + "\",\"createFolder\":false}";
                }
                return std::string(R"({"targetFolder":null,"createFolder":false})");
            }
            return request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos
                ? std::string("Images : Landscape")
                : "{\"targetFolder\":\"" + parent + "/75 Landscape\",\"createFolder\":true}";
        });
    };
    std::atomic<bool> stop{false};
    int index = 0;
    for (const auto* name : {"first.tiff", "second.heic", "third.avif"}) {
        if (cached_routes) {
            REQUIRE(database.insert_or_update_file_with_categorization(
                name, "F", source.path().string(), {0, "Images", "Landscape"}, false, ""));
            DatabaseManager::FolderTreeRoutingRecord route;
            route.destination_root = archive.path().string();
            route.tree_fingerprint = catalog.fingerprint();
            route.allow_new_folders = true;
            route.semantic_category = "Images";
            route.semantic_subcategory = "Landscape";
            route.target_folder_relative_path = parent + "/" + std::to_string(first_available + index) + " Landscape";
            route.target_folder_suggested_new = true;
            REQUIRE(database.insert_or_update_folder_tree_routing(name, "F", source.path().string(), route));
        }
        const auto result = service.categorize_entries(
            {{(source.path() / name).string(), name, FileType::File}},
            true, stop, {}, {}, {}, {}, factory, {}, {}, {}, &plan);
        REQUIRE(result.size() == 1);
        CHECK(result.front().target_folder_relative_path == expected);
        CHECK(result.front().target_folder_suggested_new);
        CHECK_FALSE(result.front().target_folder_exists);
        const auto stored = database.get_folder_tree_routing(name, FileType::File, source.path().string(),
            archive.path().string(), catalog.fingerprint(), true, "Images", "Landscape");
        REQUIRE(stored);
        CHECK(stored->target_folder_relative_path == expected);
        ++index;
    }
    CHECK(plan.planning_catalog(archive.path().string(), catalog).entries().size() == catalog.entries().size() + 1);
    CHECK_FALSE(std::filesystem::exists(archive.path() / expected));
    if (cached_routes) {
        CHECK(requests->empty());
    }

    settings.set_suggest_new_folders(false);
    const auto existing_only = service.categorize_entries(
        {{(source.path() / "first.tiff").string(), "first.tiff", FileType::File}},
        true, stop, {}, {}, {}, {}, factory, {}, {}, {}, &plan);
    REQUIRE(existing_only.size() == 1);
    CHECK(existing_only.front().target_folder_exists);
    CHECK_FALSE(existing_only.front().target_folder_suggested_new);
    CHECK(existing_only.front().target_folder_relative_path != expected);
}

TEST_CASE("CategorizationService preserves model destinations over preliminary lexical matches", "[content-routing][llm-routing]")
{
    const bool allow_new = GENERATE(false, true);
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(allow_new);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Documents" / "Reports");
    std::filesystem::create_directories(archive.path() / "Clients" / "Delivery");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    auto requests = std::make_shared<std::vector<Request>>();
    auto factory = [requests] {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            return request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos
                ? std::string("Documents : Reports")
                : std::string(R"({"targetFolder":"Clients/Delivery","createFolder":false})");
        });
    };
    std::atomic<bool> stop{false};
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto result = service.categorize_entries({{(source.path() / "monthly.pdf").string(), "monthly.pdf", FileType::File}},
            true, stop, {}, {}, {}, {}, factory);
        REQUIRE(result.size() == 1);
        CHECK(result.front().target_folder_relative_path == "Clients/Delivery");
    }
    // The second review uses the model-owned route cache without a lexical override.
    CHECK(requests->size() == 2);
}

TEST_CASE("CategorizationService leaves failed model routing unresolved instead of guessing", "[content-routing][llm-routing]")
{
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(false);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Documents" / "Reports");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    auto requests = std::make_shared<std::vector<Request>>();
    auto factory = [requests] {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            return request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos
                ? std::string("Documents : Reports") : std::string("invalid response");
        });
    };
    std::atomic<bool> stop{false};
    const auto result = service.categorize_entries({{(source.path() / "monthly.pdf").string(), "monthly.pdf", FileType::File}},
        true, stop, {}, {}, {}, {}, factory);
    CHECK(std::all_of(result.begin(), result.end(), [](const auto& entry) {
        return entry.target_folder_relative_path.empty() && entry.category.empty();
    }));
    CHECK(requests->size() == 3);
}

TEST_CASE("Folder route cache excludes legacy lexical-policy decisions", "[content-routing][llm-routing]")
{
    const std::string old_fingerprint = GENERATE("tree-hash", "content-llm-v2:tree-hash", "staged-constrained-llm-v3:tree-hash", "purpose-aware-llm-v4:tree-hash");
    TempDir config;
    DatabaseManager database(config.path().string());
    DatabaseManager::FolderTreeRoutingRecord route;
    route.destination_root = "archive";
    route.tree_fingerprint = "tree-hash";
    route.semantic_category = "Documents";
    route.semantic_subcategory = "Reports";
    route.target_folder_relative_path = "Clients/Delivery";
    route.target_folder_exists = true;
    REQUIRE(database.insert_or_update_folder_tree_routing("monthly.pdf", "F", "inbox", route));
    const auto lookup = [&] {
        return database.get_folder_tree_routing("monthly.pdf", FileType::File, "inbox", "archive",
            "tree-hash", false, "Documents", "Reports");
    };
    REQUIRE(lookup());
    CHECK(lookup()->tree_fingerprint == "tree-hash");
    sqlite3* raw = nullptr;
    const auto file = (config.path() / "categorization_results.db").string();
    REQUIRE(sqlite3_open(file.c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(raw, "UPDATE folder_tree_routing SET tree_fingerprint = ?", -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_bind_text(statement, 1, old_fingerprint.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK);
    const auto updated = sqlite3_step(statement);
    sqlite3_finalize(statement);
    sqlite3_close(raw);
    REQUIRE(updated == SQLITE_DONE);
    CHECK_FALSE(lookup());
    REQUIRE(database.insert_or_update_folder_tree_routing("monthly.pdf", "F", "inbox", route));
    REQUIRE(lookup());
    CHECK(lookup()->target_folder_relative_path == "Clients/Delivery");
}

TEST_CASE("Folder-purpose edits invalidate cached semantic routes without losing evidence", "[content-routing][folder-purpose]")
{
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Reports");
    std::filesystem::create_directories(archive.path() / "Presentations");
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(true);
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    const auto requests = std::make_shared<std::vector<Request>>();
    const auto factory = [requests] {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            if (request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos) return std::string("Business : Launch");
            if (request.context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos) {
                return std::string(R"({"targetFolder":"Presentations","createFolder":true})");
            }
            return request.context.find("Launch talks and slide decks") == std::string::npos
                ? std::string(R"({"targetFolder":"Reports","createFolder":false})")
                : std::string(R"({"targetFolder":null,"createFolder":false})");
        });
    };
    const auto path = (source.path() / "launch.pptx").string();
    const std::string evidence = "A launch presentation with demo screenshots and presenter notes.";
    const auto prompt = AnalysisCoordinator::build_document_prompt_path(path, "launch.pptx", evidence);
    std::atomic<bool> stop{false};
    const auto run = [&] {
        return service.categorize_entries({{path, "launch.pptx", FileType::File}}, true, stop, {}, {}, {}, {}, factory,
            [&](const FileEntry&) { return CategorizationService::PromptOverride{"launch.pptx", prompt}; });
    };
    const auto first = run();
    REQUIRE(first.size() == 1);
    CHECK(first.front().target_folder_relative_path == "Reports");
    const auto cached = run();
    REQUIRE(cached.size() == 1);
    CHECK(cached.front().target_folder_relative_path == "Reports");
    CHECK(requests->size() == 2);
    {
        std::ofstream file(archive.path() / FolderPurposeMetadata::kFileName);
        file << R"({"version":1,"folders":{"Reports":"Board reports","Presentations":"Launch talks and slide decks"}})";
    }
    const auto reconsidered = run();
    REQUIRE(reconsidered.size() == 1);
    CHECK(reconsidered.front().target_folder_relative_path == "Presentations");
    CHECK(reconsidered.front().target_folder_exists);
    CHECK_FALSE(reconsidered.front().target_folder_suggested_new);
    REQUIRE(requests->size() == 4);
    for (const auto& request : *requests) {
        CHECK(request.context.find(evidence) != std::string::npos);
        CHECK(request.context.find(FolderTreeSelectionContract::kReconsiderationMarker) == std::string::npos);
        CHECK(request.context.find("Purpose-aware comparison") == std::string::npos);
    }
    CHECK(requests->back().context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos);
    CHECK_FALSE(requests->back().schema.empty());
}

TEST_CASE("CategorizationService reuses literal proposals without overriding existing targets", "[content-routing][folder-plan]")
{
    const bool cached_routes = GENERATE(false, true);
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(true);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Media" / "Reference");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    const auto catalog = FolderTreeCatalog::Catalog::scan(archive.path());
    REQUIRE_FALSE(FolderStructurePattern::infer_profile(catalog).has_recognized_conventions);
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    FolderTreeRoutingPlan plan;
    auto requests = std::make_shared<std::vector<Request>>();
    const auto factory = [requests] {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            if (request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos) {
                return std::string("Images : Landscape");
            }
            if (request.context.find(FolderTreeSelectionContract::kProposalMarker) == std::string::npos) {
                if (request.context.find("Media/LANDSCAPE\n") != std::string::npos) {
                    return std::string(R"({"targetFolder":"Media/LANDSCAPE","createFolder":false})");
                }
                if (request.context.find("Media/Landscape\n") != std::string::npos) {
                    return std::string(R"({"targetFolder":"Media/Landscape","createFolder":false})");
                }
                return std::string(R"({"targetFolder":null,"createFolder":false})");
            }
            const auto target = request.name == "first.jpg" ? "Media/Landscape" : "Media/LANDSCAPE";
            return std::string("{\"targetFolder\":\"") + target + "\",\"createFolder\":true}";
        });
    };
    std::atomic<bool> stop{false};
    for (const auto* name : {"first.jpg", "second.jpg"}) {
        if (cached_routes) {
            REQUIRE(database.insert_or_update_file_with_categorization(
                name, "F", source.path().string(), {0, "Images", "Landscape"}, false, ""));
            DatabaseManager::FolderTreeRoutingRecord route;
            route.destination_root = archive.path().string();
            route.tree_fingerprint = catalog.fingerprint();
            route.allow_new_folders = true;
            route.semantic_category = "Images";
            route.semantic_subcategory = "Landscape";
            route.target_folder_relative_path = std::string(name) == "first.jpg" ? "Media/Landscape" : "Media/LANDSCAPE";
            route.target_folder_suggested_new = true;
            REQUIRE(database.insert_or_update_folder_tree_routing(name, "F", source.path().string(), route));
        }
        const auto result = service.categorize_entries(
            {{(source.path() / name).string(), name, FileType::File}},
            true, stop, {}, {}, {}, {}, factory, {}, {}, {}, &plan);
        REQUIRE(result.size() == 1);
        CHECK(result.front().target_folder_relative_path == "Media/Landscape");
        CHECK(result.front().target_folder_suggested_new);
        CHECK_FALSE(result.front().target_folder_exists);
        const auto stored = database.get_folder_tree_routing(name, FileType::File, source.path().string(),
            archive.path().string(), catalog.fingerprint(), true, "Images", "Landscape");
        REQUIRE(stored);
        CHECK(stored->target_folder_relative_path == "Media/Landscape");
    }
    CHECK(plan.planning_catalog(archive.path().string(), catalog).entries().size() == catalog.entries().size() + 1);
    CHECK_FALSE(std::filesystem::exists(archive.path() / "Media/Landscape"));
    if (cached_routes) {
        CHECK(requests->empty());
    }

    std::filesystem::create_directories(archive.path() / "Media/LANDSCAPE");
    const auto existing = service.categorize_entries(
        {{(source.path() / "third.jpg").string(), "third.jpg", FileType::File}},
        true, stop, {}, {}, {}, {}, factory, {}, {}, {}, &plan);
    REQUIRE(existing.size() == 1);
    CHECK(existing.front().target_folder_relative_path == "Media/LANDSCAPE");
    CHECK(existing.front().target_folder_exists);
    CHECK_FALSE(existing.front().target_folder_suggested_new);
}

TEST_CASE("AnalysisCoordinator reuses complete image evidence without a visual backend", "[content-routing]")
{
    enum class EvidenceState { Missing, Legacy, Current };
    const auto state = GENERATE(EvidenceState::Missing, EvidenceState::Legacy, EvidenceState::Current);
    const bool complete = state != EvidenceState::Missing;
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
    const auto version = state == EvidenceState::Current ? "image-v2:" : "image-v1:";
    const auto key = ContentAnalysisCache::make_key(image_path, version + settings.get_visual_model_id());
    REQUIRE(key);
    if (complete) {
        REQUIRE(database.content_analysis_cache().store(
            *key, {"policy_changes.png", "Customer data protection and privacy obligations."}));
    }
    const auto requests = std::make_shared<std::vector<Request>>();
    const auto run = run_cached_image_workflow(settings, database, source.path(), requests);
    INFO(run.result.error_message);
    if (state != EvidenceState::Current) {
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
    settings.set_image_filename_words(6);
    const auto different_word_limit = run_cached_image_workflow(settings, database, source.path(), requests);
    CHECK(different_word_limit.result.status == AnalysisRunStatus::Failed);
    CHECK(requests->size() == 2);
    settings.set_image_filename_words(3);
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

TEST_CASE("Content evidence preserves multiline text URLs and literal paths end to end", "[content-routing]")
{
    const bool image = GENERATE(false, true);
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Research" / "Notes");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    const std::string evidence = "Subject: parser validation\nURL: https://example.org/files//sample?q=1\n"
                                 "Network: \\\\server\\share\nDetails:  two  spaces; ../literal/reference";
    const auto path = source.path() / (image ? "image.png" : "notes.txt");
    const auto prompt = image ? AnalysisCoordinator::build_image_prompt_path(path.string(), path.filename().string(), evidence)
                              : AnalysisCoordinator::build_document_prompt_path(path.string(), path.filename().string(), evidence);
    auto requests = std::make_shared<std::vector<Request>>();
    const auto factory = [requests]() {
        return std::make_unique<RoutingLLM>(requests, [](const Request& request) {
            return request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos
                ? std::string("Research : Notes")
                : std::string(R"({"targetFolder":"Research/Notes","createFolder":false})");
        });
    };
    std::atomic<bool> stop{false};
    const auto result = service.categorize_entries({{path.string(), path.filename().string(), FileType::File}},
        true, stop, {}, {}, {}, {}, factory,
        [&](const FileEntry&) { return CategorizationService::PromptOverride{path.filename().string(), prompt}; });
    REQUIRE(result.size() == 1);
    REQUIRE(requests->size() == 2);
    CHECK(result.front().target_folder_relative_path == "Research/Notes");
    for (const auto& request : *requests) {
        CHECK(request.path.find('\n') == std::string::npos);
        CHECK(request.context.find(evidence) != std::string::npos);
    }
}

TEST_CASE("Folder destination selection survives invalid preliminary categories", "[content-routing]")
{
    const std::string preliminary = GENERATE(std::string(), std::string("Utilities : Utilities"), std::string("invalid"));
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_use_consistency_hints(false);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(false);
    TempDir source;
    TempDir archive;
    std::filesystem::create_directories(archive.path() / "Engineering" / "Software");
    settings.set_sort_folder(source.path().string());
    settings.set_destination_folder(archive.path().string());
    DatabaseManager database(settings.get_config_dir());
    CategorizationService service(settings, database, nullptr);
    auto requests = std::make_shared<std::vector<Request>>();
    const auto factory = [requests, preliminary]() {
        return std::make_unique<RoutingLLM>(requests, [preliminary](const Request& request) {
            return request.context.find(FolderTreeCatalog::kPromptMarker) == std::string::npos
                ? preliminary : std::string(R"({"targetFolder":"Engineering/Software","createFolder":false})");
        });
    };
    std::atomic<bool> stop{false};
    const std::string name = "application-linux.zip";
    const auto result = service.categorize_entries({{(source.path() / name).string(), name, FileType::File}},
        true, stop, {}, {}, {}, {}, factory);
    REQUIRE(result.size() == 1);
    REQUIRE(requests->size() == 2);
    CHECK(result.front().target_folder_relative_path == "Engineering/Software");
    CHECK(result.front().folder_tree_mode);
    CHECK(result.front().canonical_category == "Engineering");
    CHECK(result.front().canonical_subcategory == "Software");
    CHECK_FALSE(result.front().target_folder_suggested_new);
    CHECK_FALSE(std::filesystem::exists(archive.path() / "Utilities"));
}
