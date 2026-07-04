#include "HeadlessAnalysisWorkflowHost.hpp"

#include "AnalysisCoordinator.hpp"
#include "CategorizationService.hpp"
#include "CategorizationSession.hpp"
#include "DatabaseManager.hpp"
#include "GeminiClient.hpp"
#include "LLMClient.hpp"
#include "LlmCatalog.hpp"
#include "LocalLLMClient.hpp"
#include "Logger.hpp"
#include "ResultsCoordinator.hpp"
#include "UserLearningStore.hpp"
#include "Utils.hpp"
#include "WhitelistStore.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace {

std::string to_utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString translate_main_app(const char* text)
{
    return QCoreApplication::translate("MainApp", text);
}

std::shared_ptr<spdlog::logger> resolve_core_logger()
{
    if (auto logger = Logger::get_logger("core_logger")) {
        return logger;
    }
    return spdlog::default_logger();
}

} // namespace

HeadlessAnalysisWorkflowHost::HeadlessAnalysisWorkflowHost(Options options)
    : options_(std::move(options))
{
    settings_.load();
    runtime_data_dir_ = settings_.get_config_dir();
    core_logger_ = resolve_core_logger();
    db_manager_ = std::make_unique<DatabaseManager>(runtime_data_dir_);
    user_learning_store_ = std::make_unique<UserLearningStore>(runtime_data_dir_);
    whitelist_store_ = std::make_unique<WhitelistStore>(runtime_data_dir_);
    categorization_service_ =
        std::make_unique<CategorizationService>(settings_,
                                                *db_manager_,
                                                core_logger_,
                                                user_learning_store_.get());
    results_coordinator_ = std::make_unique<ResultsCoordinator>(storage_provider_);
    using_local_llm_ = !is_remote_choice(settings_.get_llm_choice());
    initialize_whitelists();
}

HeadlessAnalysisWorkflowHost::~HeadlessAnalysisWorkflowHost() = default;

AnalysisRunResult HeadlessAnalysisWorkflowHost::execute()
{
    return AnalysisCoordinator(make_context()).execute();
}

std::size_t HeadlessAnalysisWorkflowHost::review_entry_count() const
{
    return new_files_to_sort_.size();
}

const std::vector<CategorizedFile>& HeadlessAnalysisWorkflowHost::review_entries() const
{
    return new_files_to_sort_;
}

AnalysisWorkflowContext HeadlessAnalysisWorkflowHost::make_context()
{
    return AnalysisWorkflowContext{
        settings_,
        *db_manager_,
        *categorization_service_,
        *results_coordinator_,
        core_logger_,
        using_local_llm_,
        already_categorized_files_,
        new_files_with_categories_,
        files_to_categorize_,
        new_files_to_sort_,
        stop_analysis_,
        text_cpu_fallback_choice_,
        [this]() { return normalize_folder_path(options_.folder_path); },
        [](const char* text) { return translate_main_app(text); },
        [this]() { return stop_analysis_.load(); },
        [this](const std::string& message) { append_progress(message); },
        [this](const std::string& directory_path) {
            prune_empty_cached_entries_for(directory_path);
        },
        [this]() { log_cached_highlights(); },
        [this]() { log_pending_queue(); },
        [this]() { return effective_scan_options(); },
        [](const std::vector<AnalysisWorkflowContext::StagePlan>&) {},
        [](AnalysisWorkflowContext::StageId, const std::vector<FileEntry>&) {},
        [](AnalysisWorkflowContext::StageId) {},
        [](AnalysisWorkflowContext::StageId, const FileEntry&) {},
        [](AnalysisWorkflowContext::StageId, const FileEntry&) {},
        [](AnalysisWorkflowContext::StageId, const FileEntry&) {},
        [this]() { return make_llm_client(); },
        []() { return false; },
        [this](const std::string& reason) { return prompt_visual_cpu_fallback(reason); },
        [this](const std::string& reason) {
            return prompt_continue_without_visual_analysis(reason);
        },
        [this](const CategorizedFile& entry, const std::string& reason) {
            notify_recategorization_reset(entry, reason);
        }};
}

FileScanOptions HeadlessAnalysisWorkflowHost::effective_scan_options() const
{
    const bool analyze_images = settings_.get_analyze_images_by_content();
    const bool analyze_documents = settings_.get_analyze_documents_by_content();
    const bool images_only = analyze_images && settings_.get_process_images_only();
    const bool documents_only = analyze_documents && settings_.get_process_documents_only();
    if (images_only || documents_only) {
        FileScanOptions options = FileScanOptions::Files;
        if (settings_.get_include_subdirectories()) {
            options = options | FileScanOptions::Recursive;
        }
        return options;
    }

    FileScanOptions options = FileScanOptions::None;
    if (settings_.get_categorize_files()) {
        options = options | FileScanOptions::Files;
    }
    if (settings_.get_categorize_directories()) {
        options = options | FileScanOptions::Directories;
    }
    if (analyze_images || analyze_documents) {
        options = options | FileScanOptions::Files;
    }
    if (settings_.get_include_subdirectories() && has_flag(options, FileScanOptions::Files)) {
        options = options | FileScanOptions::Recursive;
    }
    return options;
}

std::unique_ptr<ILLMClient> HeadlessAnalysisWorkflowHost::make_llm_client()
{
    const LLMChoice choice = settings_.get_llm_choice();
    const auto handle_local_llm_status = [this](LocalLLMClient::Status status) {
        switch (status) {
        case LocalLLMClient::Status::GpuLowMemoryFallbackToCpu:
            append_progress(to_utf8(translate_main_app(
                "[WARN] Available GPU memory is too low for GPU acceleration. Continuing on CPU (slower).")));
            return;
        case LocalLLMClient::Status::GpuFallbackToCpu:
            append_progress(to_utf8(translate_main_app(
                "[WARN] GPU acceleration failed to initialize. Continuing on CPU (slower).")));
            return;
        }
    };

    if (choice == LLMChoice::Unset) {
        throw std::runtime_error(
            "LLM is not selected. Open the app and choose an LLM before running headless categorization.");
    }

    if (choice == LLMChoice::Remote_OpenAI) {
        const std::string api_key = settings_.get_openai_api_key();
        const std::string model = settings_.get_openai_model();
        if (api_key.empty()) {
            throw std::runtime_error("OpenAI API key is missing. Please add it from Select LLM.");
        }
        CategorizationSession session(api_key, model);
        return std::make_unique<LLMClient>(session.create_llm_client());
    }

    if (choice == LLMChoice::Remote_Gemini) {
        const std::string api_key = settings_.get_gemini_api_key();
        const std::string model = settings_.get_gemini_model();
        if (api_key.empty()) {
            throw std::runtime_error("Gemini API key is missing. Please add it from Select LLM.");
        }
        return std::make_unique<GeminiClient>(api_key, model);
    }

    if (choice == LLMChoice::Remote_Custom) {
        const auto id = settings_.get_active_custom_api_id();
        const CustomApiEndpoint endpoint = settings_.find_custom_api_endpoint(id);
        if (endpoint.id.empty() || endpoint.base_url.empty() || endpoint.model.empty()) {
            throw std::runtime_error("Selected custom API endpoint is missing or invalid. Please re-select it.");
        }
        return std::make_unique<LLMClient>(endpoint.api_key, endpoint.model, endpoint.base_url);
    }

    if (choice == LLMChoice::Custom) {
        const auto id = settings_.get_active_custom_llm_id();
        const CustomLLM custom = settings_.find_custom_llm(id);
        if (custom.id.empty() || custom.path.empty()) {
            throw std::runtime_error("Selected custom LLM is missing or invalid. Please re-select it.");
        }
        auto client = std::make_unique<LocalLLMClient>(
            custom.path,
            [this](const std::string& reason) { return prompt_text_cpu_fallback(reason); });
        client->set_status_callback(handle_local_llm_status);
        return client;
    }

    const std::filesystem::path model_path =
        resolve_downloaded_builtin_llm_path(choice).value_or(preferred_builtin_llm_path(choice));
    if (model_path.empty()) {
        throw std::runtime_error("Required local LLM model path is not configured.");
    }

    auto client = std::make_unique<LocalLLMClient>(
        Utils::path_to_utf8(model_path),
        [this](const std::string& reason) { return prompt_text_cpu_fallback(reason); });
    client->set_status_callback(handle_local_llm_status);
    return client;
}

void HeadlessAnalysisWorkflowHost::initialize_whitelists()
{
    whitelist_store_->initialize_from_settings(settings_);
    sync_whitelists_to_learning_store();
}

void HeadlessAnalysisWorkflowHost::sync_whitelists_to_learning_store()
{
    if (!user_learning_store_->is_open()) {
        return;
    }

    std::string error;
    if (!user_learning_store_->remove_taxonomy_candidates_with_source_prefix("whitelist:", &error)) {
        if (core_logger_) {
            core_logger_->warn("Failed to clear whitelist taxonomy from user learning store: {}", error);
        }
        return;
    }

    std::vector<UserLearningStore::TaxonomyCandidate> candidates;
    for (const auto& name : whitelist_store_->list_names()) {
        const auto entry = whitelist_store_->get(name);
        if (!entry) {
            continue;
        }
        std::size_t mapped_pair_count = 0;
        for (const auto& [category, subcategories] : entry->subcategories_by_category) {
            (void)category;
            mapped_pair_count += subcategories.size();
        }
        candidates.reserve(candidates.size() + entry->categories.size() + mapped_pair_count);
        const std::string source = "whitelist:" + name;
        for (const auto& category : entry->categories) {
            candidates.push_back(UserLearningStore::TaxonomyCandidate{category, std::string(), source});
        }
        for (const auto& [category, subcategories] : entry->subcategories_by_category) {
            for (const auto& subcategory : subcategories) {
                candidates.push_back(UserLearningStore::TaxonomyCandidate{category, subcategory, source});
            }
        }
    }

    if (candidates.empty()) {
        return;
    }
    if (!user_learning_store_->import_taxonomy_candidates(candidates, &error) && core_logger_) {
        core_logger_->warn("Failed to import whitelist taxonomy into user learning store: {}", error);
    }
}

void HeadlessAnalysisWorkflowHost::prune_empty_cached_entries_for(const std::string& directory_path)
{
    const std::vector<CategorizedFile> cleared =
        categorization_service_->prune_empty_cached_entries(directory_path);
    for (const auto& entry : cleared) {
        notify_recategorization_reset(
            entry,
            "Cached category was empty. The item will be analyzed again.");
    }
}

void HeadlessAnalysisWorkflowHost::log_cached_highlights()
{
    if (already_categorized_files_.empty()) {
        return;
    }

    append_progress(to_utf8(translate_main_app("[ARCHIVE] Already categorized highlights:")));
    for (const auto& entry : already_categorized_files_) {
        const QString type_label = entry.type == FileType::Directory
            ? translate_main_app("Directory")
            : translate_main_app("File");
        const QString subcategory = entry.subcategory.empty()
            ? QStringLiteral("-")
            : QString::fromStdString(entry.subcategory);
        append_progress(to_utf8(QStringLiteral("  - [%1] %2 -> %3 / %4")
                                    .arg(type_label,
                                         QString::fromStdString(entry.file_name),
                                         QString::fromStdString(entry.category),
                                         subcategory)));
    }
}

void HeadlessAnalysisWorkflowHost::log_pending_queue()
{
    if (files_to_categorize_.empty()) {
        append_progress(to_utf8(translate_main_app("[DONE] No files to categorize.")));
        return;
    }

    append_progress(to_utf8(translate_main_app("[QUEUE] Items waiting for categorization:")));
    for (const auto& entry : files_to_categorize_) {
        const QString type_label = entry.type == FileType::Directory
            ? translate_main_app("Directory")
            : translate_main_app("File");
        append_progress(to_utf8(QStringLiteral("  - [%1] %2")
                                    .arg(type_label,
                                         QString::fromStdString(entry.file_name))));
    }
}

void HeadlessAnalysisWorkflowHost::append_progress(const std::string& message)
{
    if (options_.progress_callback) {
        options_.progress_callback(message);
    }
}

bool HeadlessAnalysisWorkflowHost::prompt_text_cpu_fallback(const std::string& reason)
{
    if (text_cpu_fallback_choice_.has_value()) {
        return text_cpu_fallback_choice_.value();
    }
    text_cpu_fallback_choice_ = true;
    if (core_logger_ && !reason.empty()) {
        core_logger_->warn("Headless text GPU fallback accepted: {}", reason);
    }
    append_progress(to_utf8(translate_main_app(
        "[WARN] GPU acceleration failed. Continuing text analysis on CPU.")));
    return true;
}

bool HeadlessAnalysisWorkflowHost::prompt_visual_cpu_fallback(const std::string& reason)
{
    if (visual_cpu_fallback_choice_.has_value()) {
        return visual_cpu_fallback_choice_.value();
    }
    visual_cpu_fallback_choice_ = true;
    if (core_logger_ && !reason.empty()) {
        core_logger_->warn("Headless visual GPU fallback accepted: {}", reason);
    }
    append_progress(to_utf8(translate_main_app(
        "[WARN] Image analysis GPU initialization failed. Continuing on CPU.")));
    return true;
}

bool HeadlessAnalysisWorkflowHost::prompt_continue_without_visual_analysis(const std::string& reason)
{
    if (continue_without_visual_analysis_choice_.has_value()) {
        return continue_without_visual_analysis_choice_.value();
    }
    continue_without_visual_analysis_choice_ = true;
    if (core_logger_ && !reason.empty()) {
        core_logger_->warn("Headless continuation without visual analysis accepted: {}", reason);
    }
    append_progress(to_utf8(translate_main_app(
        "[WARN] Image analysis is unavailable. Continuing with filenames only.")));
    return true;
}

void HeadlessAnalysisWorkflowHost::notify_recategorization_reset(const CategorizedFile& entry,
                                                                 const std::string& reason)
{
    append_progress("[WARN] " + entry.file_name + " will be re-categorized: " + reason);
}

std::string HeadlessAnalysisWorkflowHost::normalize_folder_path(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path normalized = std::filesystem::absolute(path, ec);
    if (ec) {
        normalized = path;
    }
    return Utils::path_to_utf8(normalized.lexically_normal());
}
