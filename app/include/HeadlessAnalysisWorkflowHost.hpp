#pragma once

#include "AnalysisRunResult.hpp"
#include "AnalysisWorkflowContext.hpp"
#include "LocalFsProvider.hpp"
#include "Settings.hpp"
#include "Types.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class CategorizationService;
class DatabaseManager;
class ILLMClient;
class ResultsCoordinator;
class UserLearningStore;
class WhitelistStore;
namespace spdlog { class logger; }

/**
 * @brief Owns non-GUI services needed to run the analysis workflow from headless integrations.
 */
class HeadlessAnalysisWorkflowHost {
public:
    /**
     * @brief Options for a headless analysis workflow run.
     */
    struct Options {
        /** @brief Folder to analyze. */
        std::filesystem::path folder_path;
        /** @brief Optional progress callback receiving translated progress text. */
        std::function<void(const std::string&)> progress_callback;
    };

    /**
     * @brief Construct a headless workflow host.
     * @param options Headless workflow options.
     */
    explicit HeadlessAnalysisWorkflowHost(Options options);

    /**
     * @brief Destroy the headless workflow host.
     */
    ~HeadlessAnalysisWorkflowHost();

    /**
     * @brief Execute the analysis workflow.
     * @return Terminal workflow status.
     */
    AnalysisRunResult execute();

    /**
     * @brief Return the number of entries prepared for review/apply after analysis.
     * @return Count of review entries.
     */
    std::size_t review_entry_count() const;

    /**
     * @brief Return entries prepared for review/apply after analysis.
     * @return Immutable review entry list.
     */
    const std::vector<CategorizedFile>& review_entries() const;

private:
    /**
     * @brief Build the UI-neutral workflow context consumed by AnalysisCoordinator.
     * @return Workflow context with headless services and callbacks.
     */
    AnalysisWorkflowContext make_context();

    /**
     * @brief Resolve scan options from persisted settings using the GUI-equivalent rules.
     * @return Effective scan flags.
     */
    FileScanOptions effective_scan_options() const;

    /**
     * @brief Construct an LLM client from persisted settings.
     * @return Configured LLM client.
     */
    std::unique_ptr<ILLMClient> make_llm_client();

    /**
     * @brief Load whitelists and copy the active selection into settings/learning data.
     */
    void initialize_whitelists();

    /**
     * @brief Import whitelist taxonomy candidates into the user learning store.
     */
    void sync_whitelists_to_learning_store();

    /**
     * @brief Remove empty cached categorizations for a directory and report resets.
     * @param directory_path Directory cache key to prune.
     */
    void prune_empty_cached_entries_for(const std::string& directory_path);

    /**
     * @brief Emit cached categorization highlights to progress output.
     */
    void log_cached_highlights();

    /**
     * @brief Emit pending categorization queue details to progress output.
     */
    void log_pending_queue();

    /**
     * @brief Forward a progress message to the caller.
     * @param message Progress text.
     */
    void append_progress(const std::string& message);

    /**
     * @brief Decide whether a text LLM GPU failure should retry on CPU.
     * @param reason Failure reason.
     * @return True to continue on CPU.
     */
    bool prompt_text_cpu_fallback(const std::string& reason);

    /**
     * @brief Decide whether visual analysis should retry on CPU.
     * @param reason Failure reason.
     * @return True to continue on CPU.
     */
    bool prompt_visual_cpu_fallback(const std::string& reason);

    /**
     * @brief Decide whether image files should continue without visual analysis.
     * @param reason Failure reason.
     * @return True to continue using filename-only analysis.
     */
    bool prompt_continue_without_visual_analysis(const std::string& reason);

    /**
     * @brief Report that a cached entry will be analyzed again.
     * @param entry Entry being reset.
     * @param reason Reset reason.
     */
    void notify_recategorization_reset(const CategorizedFile& entry, const std::string& reason);

    /**
     * @brief Normalize a folder path for database/cache keys and prompts.
     * @param path Folder path.
     * @return UTF-8 absolute normalized path when possible.
     */
    static std::string normalize_folder_path(const std::filesystem::path& path);

    Options options_;
    Settings settings_;
    std::string runtime_data_dir_;
    LocalFsProvider storage_provider_;
    std::unique_ptr<DatabaseManager> db_manager_;
    std::unique_ptr<UserLearningStore> user_learning_store_;
    std::unique_ptr<WhitelistStore> whitelist_store_;
    std::shared_ptr<spdlog::logger> core_logger_;
    std::unique_ptr<CategorizationService> categorization_service_;
    std::unique_ptr<ResultsCoordinator> results_coordinator_;
    bool using_local_llm_{false};
    std::vector<CategorizedFile> already_categorized_files_;
    std::vector<CategorizedFile> new_files_with_categories_;
    std::vector<FileEntry> files_to_categorize_;
    std::vector<CategorizedFile> new_files_to_sort_;
    std::atomic<bool> stop_analysis_{false};
    std::optional<bool> text_cpu_fallback_choice_;
    std::optional<bool> visual_cpu_fallback_choice_;
    std::optional<bool> continue_without_visual_analysis_choice_;
};
