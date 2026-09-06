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
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>
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

std::string env_value_or_unset(const char* name)
{
    const char* value = std::getenv(name);
    if (!value || value[0] == '\0') {
        return "<unset>";
    }
    return value;
}

std::string text_llm_backend_request_summary()
{
    return "[INFO] Local text LLM backend request: AI_FILE_SORTER_GPU_BACKEND=" +
           env_value_or_unset("AI_FILE_SORTER_GPU_BACKEND") +
           ", LLAMA_ARG_DEVICE=" + env_value_or_unset("LLAMA_ARG_DEVICE") +
           ", GGML_DISABLE_CUDA=" + env_value_or_unset("GGML_DISABLE_CUDA") +
           ", AI_FILE_SORTER_N_GPU_LAYERS=" + env_value_or_unset("AI_FILE_SORTER_N_GPU_LAYERS") +
           ", LLAMA_CPP_N_GPU_LAYERS=" + env_value_or_unset("LLAMA_CPP_N_GPU_LAYERS") +
           ", AI_FILE_SORTER_GGML_DIR=" + env_value_or_unset("AI_FILE_SORTER_GGML_DIR");
}

} // namespace

HeadlessAnalysisWorkflowHost::HeadlessAnalysisWorkflowHost(Options options)
    : options_(std::move(options))
{
    settings_.load();
    settings_.set_sort_folder(normalize_folder_path(options_.folder_path));
    // Headless callers default to the analyzed root unless they explicitly override it.
    settings_.set_destination_folder(std::string());
    if (options_.include_subdirectories) {
        settings_.set_include_subdirectories(*options_.include_subdirectories);
    }
    apply_settings_overrides();
    apply_operation_settings_overlay();
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
    AnalysisRunResult result = AnalysisCoordinator(make_context()).execute();
    if (result.status == AnalysisRunStatus::Completed) {
        filter_review_entries_to_selected_paths();
    }
    return result;
}

void HeadlessAnalysisWorkflowHost::request_stop()
{
    stop_analysis_.store(true);
}

std::size_t HeadlessAnalysisWorkflowHost::review_entry_count() const
{
    return new_files_to_sort_.size();
}

const std::vector<CategorizedFile>& HeadlessAnalysisWorkflowHost::review_entries() const
{
    return new_files_to_sort_;
}

std::vector<CategorizedFile> HeadlessAnalysisWorkflowHost::preview_review_entries() const
{
    std::vector<CategorizedFile> entries = already_categorized_files_;
    entries.reserve(entries.size() + new_files_with_categories_.size());
    entries.insert(entries.end(),
                   new_files_with_categories_.begin(),
                   new_files_with_categories_.end());

    std::unordered_set<std::string> seen;
    seen.reserve(entries.size());
    entries.erase(
        std::remove_if(entries.begin(),
                       entries.end(),
                       [&seen](const CategorizedFile& entry) {
                           const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                                  Utils::utf8_to_path(entry.file_name);
                           return !seen.insert(HeadlessAnalysisWorkflowHost::selected_path_key(full_path)).second;
                       }),
        entries.end());

    if (options_.selected_paths.empty()) {
        return entries;
    }

    std::unordered_set<std::string> selected;
    selected.reserve(options_.selected_paths.size());
    for (const auto& path : options_.selected_paths) {
        selected.insert(selected_path_key(path));
    }

    entries.erase(
        std::remove_if(entries.begin(),
                       entries.end(),
                       [&selected](const CategorizedFile& entry) {
                           const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                                  Utils::utf8_to_path(entry.file_name);
                           return !selected.contains(HeadlessAnalysisWorkflowHost::selected_path_key(full_path));
                       }),
        entries.end());
    return entries;
}

std::string HeadlessAnalysisWorkflowHost::last_progress_message() const
{
    return last_progress_message_;
}

std::string HeadlessAnalysisWorkflowHost::folder_path() const
{
    return normalize_folder_path(options_.folder_path);
}

std::string HeadlessAnalysisWorkflowHost::destination_folder() const
{
    return settings_.get_effective_destination_folder(folder_path());
}

bool HeadlessAnalysisWorkflowHost::use_subcategories() const
{
    return settings_.get_use_subcategories();
}

bool HeadlessAnalysisWorkflowHost::include_subdirectories() const
{
    return settings_.get_include_subdirectories();
}

bool HeadlessAnalysisWorkflowHost::suggest_new_folders() const
{
    return settings_.get_suggest_new_folders();
}

CategoryLanguage HeadlessAnalysisWorkflowHost::category_language() const
{
    return settings_.get_category_language();
}

bool HeadlessAnalysisWorkflowHost::headless_review_before_apply() const
{
    return settings_.get_headless_review_before_apply();
}

std::string HeadlessAnalysisWorkflowHost::undo_dir() const
{
    const auto undo_path = Utils::utf8_to_path(runtime_data_dir_) / "undo";
    return Utils::path_to_utf8(undo_path);
}

DatabaseManager& HeadlessAnalysisWorkflowHost::db_manager()
{
    return *db_manager_;
}

IStorageProvider& HeadlessAnalysisWorkflowHost::storage_provider()
{
    return storage_provider_;
}

std::shared_ptr<spdlog::logger> HeadlessAnalysisWorkflowHost::core_logger() const
{
    return core_logger_;
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
        [this]() { return should_stop(); },
        [this](const std::string& message) { append_progress(message); },
        [this](const std::string& directory_path) {
            prune_empty_cached_entries_for(directory_path);
        },
        [this]() { log_cached_highlights(); },
        [this]() { log_pending_queue(); },
        [this]() { return effective_scan_options(); },
        [this](std::vector<FileEntry>& entries) {
            filter_file_entries_to_selected_paths(entries);
        },
        [this]() { notify_review_preview_changed(); },
        [](const std::vector<AnalysisWorkflowContext::StagePlan>&) {},
        [](AnalysisWorkflowContext::StageId, const std::vector<FileEntry>&) {},
        [](AnalysisWorkflowContext::StageId) {},
        [](AnalysisWorkflowContext::StageId, const FileEntry&) {},
        [](AnalysisWorkflowContext::StageId, const FileEntry&) {},
        [](AnalysisWorkflowContext::StageId, const FileEntry&) {},
        [this]() { return make_llm_client(); },
        []() { return false; },
        options_.operation_mode != OperationMode::Rename,
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
    if (options_.operation_mode == OperationMode::Rename) {
        FileScanOptions options = FileScanOptions::Files;
        if (settings_.get_include_subdirectories()) {
            options = options | FileScanOptions::Recursive;
        }
        return options;
    }

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
        append_progress(text_llm_backend_request_summary());
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

    append_progress(text_llm_backend_request_summary());
    auto client = std::make_unique<LocalLLMClient>(
        Utils::path_to_utf8(model_path),
        [this](const std::string& reason) { return prompt_text_cpu_fallback(reason); });
    client->set_status_callback(handle_local_llm_status);
    return client;
}

void HeadlessAnalysisWorkflowHost::initialize_whitelists()
{
    whitelist_store_->initialize_from_settings(settings_);
    apply_direct_whitelist_overrides();
    sync_whitelists_to_learning_store();
}

void HeadlessAnalysisWorkflowHost::apply_direct_whitelist_overrides()
{
    const HeadlessSettingsOverrides& overrides = options_.settings_overrides;
    if (overrides.allowed_subcategories) {
        settings_.set_allowed_subcategories_by_category({});
    }
    if (overrides.allowed_categories) {
        settings_.set_allowed_categories(*overrides.allowed_categories);
    }
    if (overrides.allowed_subcategories) {
        settings_.set_allowed_subcategories(*overrides.allowed_subcategories);
    }
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

void HeadlessAnalysisWorkflowHost::apply_operation_settings_overlay()
{
    settings_.set_process_images_only(false);
    settings_.set_process_documents_only(false);

    switch (options_.operation_mode) {
    case OperationMode::Rename:
        settings_.set_categorize_files(false);
        settings_.set_categorize_directories(false);
        settings_.set_offer_rename_images(true);
        settings_.set_offer_rename_documents(true);
        settings_.set_rename_images_only(true);
        settings_.set_rename_documents_only(true);
        settings_.set_add_image_date_to_category(false);
        settings_.set_add_document_date_to_category(false);
        return;
    case OperationMode::CategorizeAndRename:
        settings_.set_offer_rename_images(true);
        settings_.set_offer_rename_documents(true);
        settings_.set_rename_images_only(false);
        settings_.set_rename_documents_only(false);
        return;
    case OperationMode::Categorize:
    default:
        settings_.set_offer_rename_images(false);
        settings_.set_offer_rename_documents(false);
        settings_.set_rename_images_only(false);
        settings_.set_rename_documents_only(false);
        return;
    }
}

void HeadlessAnalysisWorkflowHost::apply_settings_overrides()
{
    const HeadlessSettingsOverrides& overrides = options_.settings_overrides;
    if (overrides.use_subcategories) {
        settings_.set_use_subcategories(*overrides.use_subcategories);
    }
    if (overrides.sorting_mode) {
        settings_.set_sorting_mode(*overrides.sorting_mode);
    }
    if (overrides.destination_folder) {
        settings_.set_destination_folder(overrides.destination_folder->empty()
                                             ? std::string()
                                             : normalize_folder_path(
                                                   Utils::utf8_to_path(*overrides.destination_folder)));
    }
    if (overrides.suggest_new_folders) {
        settings_.set_suggest_new_folders(*overrides.suggest_new_folders);
    }
    if (overrides.use_consistency_hints) {
        settings_.set_use_consistency_hints(*overrides.use_consistency_hints);
    }
    if (overrides.use_whitelist) {
        settings_.set_use_whitelist(*overrides.use_whitelist);
    }
    if (overrides.active_whitelist) {
        settings_.set_active_whitelist(*overrides.active_whitelist);
    }
    if (overrides.allowed_categories || overrides.allowed_subcategories) {
        apply_direct_whitelist_overrides();
    }
    if (overrides.categorize_files) {
        settings_.set_categorize_files(*overrides.categorize_files);
    }
    if (overrides.categorize_directories) {
        settings_.set_categorize_directories(*overrides.categorize_directories);
    }
    if (overrides.include_subdirectories) {
        settings_.set_include_subdirectories(*overrides.include_subdirectories);
    }
    if (overrides.analyze_images_by_content) {
        settings_.set_analyze_images_by_content(*overrides.analyze_images_by_content);
    }
    if (overrides.process_images_only) {
        settings_.set_process_images_only(*overrides.process_images_only);
    }
    if (overrides.add_image_date_to_category) {
        settings_.set_add_image_date_to_category(*overrides.add_image_date_to_category);
    }
    if (overrides.add_image_date_place_to_filename) {
        settings_.set_add_image_date_place_to_filename(*overrides.add_image_date_place_to_filename);
    }
    if (overrides.offer_rename_images) {
        settings_.set_offer_rename_images(*overrides.offer_rename_images);
    }
    if (overrides.rename_images_only) {
        settings_.set_rename_images_only(*overrides.rename_images_only);
    }
    if (overrides.add_audio_video_metadata_to_filename) {
        settings_.set_add_audio_video_metadata_to_filename(*overrides.add_audio_video_metadata_to_filename);
    }
    if (overrides.analyze_documents_by_content) {
        settings_.set_analyze_documents_by_content(*overrides.analyze_documents_by_content);
    }
    if (overrides.process_documents_only) {
        settings_.set_process_documents_only(*overrides.process_documents_only);
    }
    if (overrides.offer_rename_documents) {
        settings_.set_offer_rename_documents(*overrides.offer_rename_documents);
    }
    if (overrides.rename_documents_only) {
        settings_.set_rename_documents_only(*overrides.rename_documents_only);
    }
    if (overrides.add_document_date_to_category) {
        settings_.set_add_document_date_to_category(*overrides.add_document_date_to_category);
    }
    if (overrides.language) {
        settings_.set_language(*overrides.language);
    }
    if (overrides.category_language) {
        settings_.set_category_language(*overrides.category_language);
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

void HeadlessAnalysisWorkflowHost::filter_file_entries_to_selected_paths(
    std::vector<FileEntry>& entries) const
{
    if (options_.selected_paths.empty()) {
        return;
    }

    std::unordered_set<std::string> selected;
    selected.reserve(options_.selected_paths.size());
    for (const auto& path : options_.selected_paths) {
        selected.insert(selected_path_key(path));
    }

    entries.erase(
        std::remove_if(entries.begin(),
                       entries.end(),
                       [&selected](const FileEntry& entry) {
                           return !selected.contains(selected_path_key(
                               Utils::utf8_to_path(entry.full_path)));
                       }),
        entries.end());
}

void HeadlessAnalysisWorkflowHost::append_progress(const std::string& message)
{
    should_stop();
    last_progress_message_ = message;
    if (options_.progress_callback) {
        options_.progress_callback(message);
    }
}

void HeadlessAnalysisWorkflowHost::notify_review_preview_changed()
{
    if (options_.review_preview_callback) {
        options_.review_preview_callback();
    }
}

bool HeadlessAnalysisWorkflowHost::should_stop()
{
    if (stop_analysis_.load()) {
        return true;
    }
    if (options_.stop_requested && options_.stop_requested()) {
        request_stop();
        return true;
    }
    return false;
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
    std::string message = to_utf8(translate_main_app(
        "[WARN] GPU acceleration failed. Continuing text analysis on CPU."));
    if (!reason.empty()) {
        message += " Reason: " + reason + ".";
    }
    append_progress(message);
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

void HeadlessAnalysisWorkflowHost::filter_review_entries_to_selected_paths()
{
    if (options_.selected_paths.empty()) {
        return;
    }

    std::unordered_set<std::string> selected;
    selected.reserve(options_.selected_paths.size());
    for (const auto& path : options_.selected_paths) {
        selected.insert(selected_path_key(path));
    }

    new_files_to_sort_.erase(
        std::remove_if(new_files_to_sort_.begin(),
                       new_files_to_sort_.end(),
                       [&selected](const CategorizedFile& entry) {
                           const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                                  Utils::utf8_to_path(entry.file_name);
                           return !selected.contains(HeadlessAnalysisWorkflowHost::selected_path_key(full_path));
                       }),
        new_files_to_sort_.end());
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

std::string HeadlessAnalysisWorkflowHost::selected_path_key(const std::filesystem::path& path)
{
    std::string key = normalize_folder_path(path);
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
#endif
    return key;
}
