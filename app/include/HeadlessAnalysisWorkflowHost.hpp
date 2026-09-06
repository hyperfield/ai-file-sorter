#pragma once

#include "AnalysisRunResult.hpp"
#include "AnalysisWorkflowContext.hpp"
#include "CategoryLanguage.hpp"
#include "HeadlessSettingsOverrides.hpp"
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
class IStorageProvider;
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
     * @brief Headless operation semantics applied as an in-memory settings overlay.
     */
    enum class OperationMode {
        Categorize,
        Rename,
        CategorizeAndRename
    };

    /**
     * @brief Options for a headless analysis workflow run.
     */
    struct Options {
        /** @brief Folder to analyze. */
        std::filesystem::path folder_path;
        /** @brief Optional same-folder file paths to keep in the review/apply result. */
        std::vector<std::filesystem::path> selected_paths;
        /** @brief Requested operation mode for this run. */
        OperationMode operation_mode{OperationMode::Categorize};
        /** @brief Optional override for recursive scanning in headless integrations. */
        std::optional<bool> include_subdirectories;
        /** @brief Optional non-persistent settings overlay for headless integrations. */
        HeadlessSettingsOverrides settings_overrides;
        /** @brief Optional progress callback receiving translated progress text. */
        std::function<void(const std::string&)> progress_callback;
        /** @brief Optional callback when partial review rows become available. */
        std::function<void()> review_preview_callback;
        /** @brief Optional external cancellation callback for headless integrations. */
        std::function<bool()> stop_requested;
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
     * @brief Request cooperative cancellation of the running headless workflow.
     */
    void request_stop();

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

    /**
     * @brief Return currently known review/apply entries while analysis is still running.
     * @return Snapshot of entries prepared so far.
     */
    std::vector<CategorizedFile> preview_review_entries() const;

    /**
     * @brief Return the most recent headless progress message.
     * @return Last progress text sent through the host callback.
     */
    std::string last_progress_message() const;

    /**
     * @brief Return the normalized selected folder path.
     * @return UTF-8 folder path used by the workflow.
     */
    std::string folder_path() const;

    /**
     * @brief Return the normalized destination root for category moves.
     * @return UTF-8 destination folder path, falling back to the analyzed folder.
     */
    std::string destination_folder() const;

    /**
     * @brief Return whether subcategory folders should be created.
     * @return True when subcategory folders are enabled.
     */
    bool use_subcategories() const;

    /**
     * @brief Return whether the workflow scanned subdirectories.
     * @return True when recursive scanning is enabled.
     */
    bool include_subdirectories() const;

    /**
     * @brief Return whether existing-folder sorting may suggest new folders.
     * @return True when new target folders may be created after review approval.
     */
    bool suggest_new_folders() const;

    /**
     * @brief Return the category language configured for the run.
     * @return Category language.
     */
    CategoryLanguage category_language() const;

    /**
     * @brief Return whether headless runs should stop for review before applying changes.
     * @return True when headless review is required before filesystem changes.
     */
    bool headless_review_before_apply() const;

    /**
     * @brief Return the undo-plan directory for headless moves.
     * @return UTF-8 undo directory path.
     */
    std::string undo_dir() const;

    /**
     * @brief Return the database manager used by the workflow.
     * @return Database manager reference.
     */
    DatabaseManager& db_manager();

    /**
     * @brief Return the storage provider used for local filesystem moves.
     * @return Storage provider reference.
     */
    IStorageProvider& storage_provider();

    /**
     * @brief Return the core logger used by the workflow.
     * @return Shared logger.
     */
    std::shared_ptr<spdlog::logger> core_logger() const;

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
     * @brief Apply direct whitelist list overrides after named whitelist loading.
     */
    void apply_direct_whitelist_overrides();

    /**
     * @brief Apply integration-provided settings without persisting them.
     */
    void apply_settings_overrides();

    /**
     * @brief Apply command-specific behavior without persisting user settings.
     */
    void apply_operation_settings_overlay();

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
     * @brief Filter scanned entries to the optional selected file set.
     * @param entries Entries from a folder scan.
     */
    void filter_file_entries_to_selected_paths(std::vector<FileEntry>& entries) const;

    /**
     * @brief Forward a progress message to the caller.
     * @param message Progress text.
     */
    void append_progress(const std::string& message);

    /**
     * @brief Notify the caller that the in-progress review preview changed.
     */
    void notify_review_preview_changed();

    /**
     * @brief Return whether the workflow should stop and latch the shared stop flag.
     * @return True when cancellation has been requested.
     */
    bool should_stop();

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
     * @brief Filter review entries to explicitly selected file paths.
     */
    void filter_review_entries_to_selected_paths();

    /**
     * @brief Normalize a folder path for database/cache keys and prompts.
     * @param path Folder path.
     * @return UTF-8 absolute normalized path when possible.
     */
    static std::string normalize_folder_path(const std::filesystem::path& path);

    /**
     * @brief Normalize a path for exact selected-file matching.
     * @param path Path to normalize.
     * @return Case-normalized UTF-8 comparison key.
     */
    static std::string selected_path_key(const std::filesystem::path& path);

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
    std::string last_progress_message_;
};
