#pragma once

#include "AnalysisProgress.hpp"
#include "Types.hpp"

#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class CategorizationService;
class DatabaseManager;
class ILLMClient;
class ResultsCoordinator;
class Settings;
namespace spdlog { class logger; }

/**
 * @brief UI-neutral dependency bundle used by the analysis workflow.
 *
 * The workflow still needs application services, mutable result buffers, and a
 * few host callbacks. Keeping those requirements explicit lets GUI and future
 * headless hosts provide the same contract without making the workflow depend
 * on the concrete main-window type.
 */
struct AnalysisWorkflowContext {
    using StageId = AnalysisProgressStageId;
    using StagePlan = AnalysisProgressStagePlan;

    Settings& settings;
    DatabaseManager& db_manager;
    CategorizationService& categorization_service;
    ResultsCoordinator& results_coordinator;
    std::shared_ptr<spdlog::logger> core_logger;
    bool& using_local_llm;
    std::vector<CategorizedFile>& already_categorized_files;
    std::vector<CategorizedFile>& new_files_with_categories;
    std::vector<FileEntry>& files_to_categorize;
    std::vector<CategorizedFile>& new_files_to_sort;
    std::atomic<bool>& stop_analysis;
    std::optional<bool>& text_cpu_fallback_choice_;

    std::function<std::string()> get_folder_path;
    std::function<QString(const char*)> tr;
    std::function<bool()> should_abort_analysis;
    std::function<void(const std::string&)> append_progress;
    std::function<void(const std::string&)> prune_empty_cached_entries_for;
    std::function<void()> log_cached_highlights;
    std::function<void()> log_pending_queue;
    std::function<FileScanOptions()> effective_scan_options;
    std::function<void(const std::vector<StagePlan>&)> configure_progress_stages;
    std::function<void(StageId, const std::vector<FileEntry>&)> set_progress_stage_items;
    std::function<void(StageId)> set_progress_active_stage;
    std::function<void(StageId, const FileEntry&)> mark_progress_stage_item_in_progress;
    std::function<void(StageId, const FileEntry&)> mark_progress_stage_item_completed;
    std::function<void(StageId, const FileEntry&)> mark_progress_stage_item_skipped;
    std::function<std::unique_ptr<ILLMClient>()> make_llm_client;
    std::function<bool()> should_log_prompts;
    std::function<bool(const std::string&)> prompt_visual_cpu_fallback;
    std::function<bool(const std::string&)> prompt_continue_without_visual_analysis;
    std::function<void(const CategorizedFile&, const std::string&)> notify_recategorization_reset;
};
