#ifndef CATEGORIZATION_SERVICE_HPP
#define CATEGORIZATION_SERVICE_HPP

#include "Types.hpp"
#include "BoundedWorkExecutor.hpp"
#include "DatabaseManager.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <exception>
#include <future>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Settings;
class ILLMClient;
class UserLearningStore;
namespace spdlog { class logger; }

/**
 * @brief Provides LLM-backed file categorization with caching and validation.
 */
class CategorizationService {
public:
    using ProgressCallback = std::function<void(const std::string&)>;
    using QueueCallback = std::function<void(const FileEntry&)>;
    using CompletionCallback = std::function<void(const FileEntry&)>;
    using ResultCallback = std::function<void(const CategorizedFile&)>;
    using RecategorizationCallback = std::function<void(const CategorizedFile&, const std::string&)>;
    /**
     * @brief Overrides the name/path used in LLM prompts for a file entry.
     */
    struct PromptOverride {
        std::string name;
        std::string path;
    };
    using PromptOverrideProvider = std::function<std::optional<PromptOverride>(const FileEntry&)>;
    /** Supplies an optional suggested rename for an entry during categorization. */
    using SuggestedNameProvider = std::function<std::string(const FileEntry&)>;

    /**
     * @brief Constructs the service with settings, database access, and logging.
     * @param settings Application settings reference.
     * @param db_manager Database manager used for cache access.
     * @param core_logger Logger for core activity.
     */
    CategorizationService(Settings& settings,
                          DatabaseManager& db_manager,
                          std::shared_ptr<spdlog::logger> core_logger,
                          UserLearningStore* user_learning_store = nullptr);

    /**
     * @brief Updates the optional learned-behavior store used for candidate retrieval.
     * @param store User-learning store, or nullptr to disable retrieval.
     */
    void set_user_learning_store(UserLearningStore* store) { user_learning_store_ = store; }

    /**
     * @brief Verifies that required remote credentials are configured.
     * @param error_message Optional output for a user-facing error message.
     * @return True when credentials are present or not required.
     */
    bool ensure_remote_credentials(std::string* error_message = nullptr) const;
    /**
     * @brief Removes cached entries that have empty categories for a directory.
     * @param directory_path Directory to clean.
     * @return Entries that were removed.
     */
    std::vector<CategorizedFile> prune_empty_cached_entries(const std::string& directory_path);
    /**
     * @brief Loads cached categorizations for the provided directory.
     * @param directory_path Directory to load.
     * @return Cached entries for the directory.
     */
    std::vector<CategorizedFile> load_cached_entries(const std::string& directory_path) const;

    /**
     * @brief Categorizes a list of file entries using the configured LLM workflow.
     * @param files Entries to categorize.
     * @param is_local_llm True when using a local LLM backend.
     * @param stop_flag Cancellation flag.
     * @param progress_callback Progress updates callback.
     * @param queue_callback Called when an entry is queued.
     * @param completion_callback Called when an entry has finished processing.
     * @param result_callback Called when an entry produced a categorizable review row.
     * @param recategorization_callback Called when an entry must be re-categorized.
     * @param llm_factory Factory for creating an LLM client.
     * @param prompt_override Optional prompt override provider.
     * @param suggested_name_provider Optional suggested-name provider.
     * @return Categorized entries that were successfully processed.
     */
    std::vector<CategorizedFile> categorize_entries(
        const std::vector<FileEntry>& files,
        bool is_local_llm,
        std::atomic<bool>& stop_flag,
        const ProgressCallback& progress_callback,
        const QueueCallback& queue_callback,
        const CompletionCallback& completion_callback,
        const RecategorizationCallback& recategorization_callback,
        std::function<std::unique_ptr<ILLMClient>()> llm_factory,
        const PromptOverrideProvider& prompt_override = {},
        const SuggestedNameProvider& suggested_name_provider = {},
        const ResultCallback& result_callback = {}) const;

private:
    using CategoryPair = std::pair<std::string, std::string>;
    using HintHistory = std::deque<CategoryPair>;
    using SessionHistoryMap = std::unordered_map<std::string, HintHistory>;
    using RemoteThrottleCallback = std::function<bool(const std::string&)>;

    /**
     * @brief Everything needed to categorize one entry, computed on the coordinator thread.
     *
     * Built from immutable inputs and a session-history snapshot before any request is
     * dispatched, so worker threads never read mutable categorization state.
     */
    struct PreparedEntry {
        FileEntry entry;
        std::string suggested_name;
        bool use_consistency_hints{false};
        std::string dir_path;
        std::string display_path;
        std::string prompt_name;
        std::string prompt_path;          ///< Raw prompt path (learning context source).
        std::string prompt_path_display;  ///< Abbreviated prompt path used in requests.
        std::string combined_context;
    };

    /**
     * @brief Raw LLM output for one prepared entry, produced on a worker thread.
     */
    struct LlmResponse {
        std::string raw;
        /// Rate-limit notices to be shown by the coordinator in order.
        std::vector<std::string> notes;
        /// True when stop was requested while waiting out a rate limit.
        bool cancelled{false};
    };

    /**
     * @brief Caller-supplied callbacks for one categorization run.
     */
    struct EntryCallbacks {
        const ProgressCallback& progress;
        const QueueCallback& queue;
        const CompletionCallback& completion;
        const RecategorizationCallback& recategorization;
        const PromptOverrideProvider& prompt_override;
        const SuggestedNameProvider& suggested_name_provider;
        const ResultCallback& result;
    };

    /**
     * @brief Per-entry state for one chunk of the concurrent categorization path.
     *
     * Items that need no request carry their resolved category directly. The others set
     * needs_request and, once queued, refer to their slot in the chunk's request list.
     */
    struct ConcurrentChunkItem {
        PreparedEntry prepared;
        DatabaseManager::ResolvedCategory resolved;
        bool cache_hit{false};
        std::optional<size_t> request_index;
        bool needs_request{false};
    };

    /**
     * @brief Items and LLM requests for one chunk of the concurrent categorization path.
     *
     * Each item whose request_index is set refers to the entry at that index in requests.
     */
    struct ConcurrentChunk {
        std::vector<ConcurrentChunkItem> items;
        std::vector<PreparedEntry> requests;
    };

    /**
     * @brief Returns a cached categorization when available, otherwise calls the LLM.
     * @param llm LLM client used for the request.
     * @param is_local_llm True when using a local LLM backend.
     * @param display_name Display name for logging.
     * @param display_path Display path for logging.
     * @param dir_path Full directory path for cache lookup.
     * @param prompt_name Name used in the prompt.
     * @param prompt_path Path used in the prompt.
     * @param file_type File or directory.
     * @param progress_callback Progress updates callback.
     * @param consistency_context Consistency hints block.
     * @param remote_throttle_callback Optional callback invoked before remote cache misses.
     * @return Resolved category for the item.
     */
    DatabaseManager::ResolvedCategory categorize_with_cache(
        ILLMClient& llm,
        bool is_local_llm,
        const std::string& display_name,
        const std::string& display_path,
        const std::string& dir_path,
        const std::string& prompt_name,
        const std::string& prompt_path,
        FileType file_type,
        const ProgressCallback& progress_callback,
        const std::string& consistency_context,
        const RemoteThrottleCallback& remote_throttle_callback) const;

    /**
     * @brief Categorizes a single entry and persists the result.
     * @param llm LLM client used for the request.
     * @param is_local_llm True when using a local LLM backend.
     * @param entry File entry to categorize.
     * @param prompt_override Optional prompt override.
     * @param suggested_name Optional suggested name for renaming.
     * @param stop_flag Cancellation flag.
     * @param progress_callback Progress updates callback.
     * @param recategorization_callback Callback for re-categorization events.
     * @param session_history Mutable session history for consistency hints.
     * @param remote_throttle_callback Optional callback invoked before remote cache misses.
     * @return Categorized entry when successful.
     */
    std::optional<CategorizedFile> categorize_single_entry(
        ILLMClient& llm,
        bool is_local_llm,
        const FileEntry& entry,
        const std::optional<PromptOverride>& prompt_override,
        const std::string& suggested_name,
        std::atomic<bool>& stop_flag,
        const ProgressCallback& progress_callback,
        const RecategorizationCallback& recategorization_callback,
        SessionHistoryMap& session_history,
        const RemoteThrottleCallback& remote_throttle_callback) const;

    /**
     * @brief Combines language, family-candidate, whitelist, and hint blocks into a single prompt context.
     * @param hint_block Consistency hint block.
     * @param prompt_name Name used in the categorization prompt.
     * @param prompt_path Path/context payload used in the categorization prompt.
     * @param file_type File or directory being categorized.
     * @return Combined prompt context.
     */
    std::string build_combined_context(const std::string& hint_block,
                                       const std::string& prompt_name = {},
                                       const std::string& prompt_path = {},
                                       FileType file_type = FileType::File) const;
    DatabaseManager::ResolvedCategory localize_resolved_category(
        ILLMClient& llm,
        const DatabaseManager::ResolvedCategory& resolved) const;
    std::optional<DatabaseManager::ResolvedCategory> translate_resolved_category(
        ILLMClient& llm,
        const DatabaseManager::ResolvedCategory& resolved) const;
    /**
     * @brief Runs the categorization flow with cache handling for a single entry.
     * @param llm LLM client used for the request.
     * @param is_local_llm True when using a local LLM backend.
     * @param entry File entry to categorize.
     * @param display_path Display path for logging.
     * @param dir_path Full directory path for cache lookup.
     * @param prompt_name Name used in the prompt.
     * @param prompt_path Path used in the prompt.
     * @param progress_callback Progress updates callback.
     * @param combined_context Combined prompt context.
     * @param remote_throttle_callback Optional callback invoked before remote cache misses.
     * @return Resolved category for the item.
     */
    DatabaseManager::ResolvedCategory run_categorization_with_cache(
        ILLMClient& llm,
        bool is_local_llm,
        const FileEntry& entry,
        const std::string& display_path,
        const std::string& dir_path,
        const std::string& prompt_name,
        const std::string& prompt_path,
        const ProgressCallback& progress_callback,
        const std::string& combined_context,
        const RemoteThrottleCallback& remote_throttle_callback) const;
    /**
     * @brief Handles empty or invalid categorization results.
     * @param entry File entry being categorized.
     * @param dir_path Directory path of the entry.
     * @param resolved Resolved category data.
     * @param used_consistency_hints True if hints were applied.
     * @param is_local_llm True when using a local LLM backend.
     * @param recategorization_callback Callback for re-categorization events.
     * @return Optional replacement categorization when a retry is needed.
     */
    std::optional<CategorizedFile> handle_empty_result(
        const FileEntry& entry,
        const std::string& dir_path,
        const DatabaseManager::ResolvedCategory& resolved,
        bool used_consistency_hints,
        bool is_local_llm,
        const RecategorizationCallback& recategorization_callback) const;
    /**
     * @brief Persists categorization results and updates session hint history.
     * @param entry File entry being categorized.
     * @param dir_path Directory path of the entry.
     * @param resolved Resolved category data.
     * @param used_consistency_hints True if hints were applied.
     * @param suggested_name Suggested rename value.
     * @param session_history Session history for consistency hints.
     */
    void update_storage_with_result(const FileEntry& entry,
                                    const std::string& dir_path,
                                    const DatabaseManager::ResolvedCategory& resolved,
                                    bool used_consistency_hints,
                                    const std::string& suggested_name,
                                    SessionHistoryMap& session_history) const;

    /**
     * @brief Runs the LLM request with a timeout for the given item.
     * @param llm LLM client used for the request.
     * @param item_name Display name for the item.
     * @param item_path Display path for the item.
     * @param file_type File or directory.
     * @param is_local_llm True when using a local LLM backend.
     * @param consistency_context Consistency hints block.
     * @return Raw LLM response string.
     */
    std::string run_llm_with_timeout(
        ILLMClient& llm,
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        bool is_local_llm,
        const std::string& consistency_context) const;
    /**
     * @brief Resolves the LLM timeout based on runtime and environment settings.
     * @param is_local_llm True when using a local LLM backend.
     * @return Timeout in seconds.
     */
    int resolve_llm_timeout(bool is_local_llm) const;
    /**
     * @brief Resolves the optional remote request throttle.
     * @return Maximum remote requests per minute, or 0 when disabled.
     */
    int resolve_remote_requests_per_minute() const;
    /**
     * @brief Launches an asynchronous LLM categorization request.
     * @param llm LLM client used for the request.
     * @param item_name Display name for the item.
     * @param item_path Display path for the item.
     * @param file_type File or directory.
     * @param consistency_context Consistency hints block.
     * @return Future that yields the raw LLM response.
     */
    std::future<std::string> start_llm_future(ILLMClient& llm,
                                              const std::string& item_name,
                                              const std::string& item_path,
                                              FileType file_type,
                                              const std::string& consistency_context) const;
    /**
     * @brief Builds a whitelist context block for the prompt.
     * @return Whitelist prompt section.
     */
    std::string build_whitelist_context() const;
    /**
     * @brief Builds a prompt block with main-category candidates narrowed by file family.
     * @param prompt_name Name used in the categorization prompt.
     * @param file_type File or directory being categorized.
     * @return Candidate main-category prompt section, or empty when unrestricted.
     */
    std::string build_main_category_candidate_context(const std::string& prompt_name,
                                                      FileType file_type) const;
    /**
     * @brief Builds the effective whitelist prompt block for a specific file context.
     * @param prompt_name Name used in the categorization prompt.
     * @param prompt_path Path/context payload used in the categorization prompt.
     * @return Full whitelist block for small lists or retrieved candidates for large lists.
     */
    std::string build_whitelist_context_for_prompt(const std::string& prompt_name,
                                                   const std::string& prompt_path) const;
    /**
     * @brief Builds a compact candidate block for very large whitelists.
     * @param prompt_name Name used in the categorization prompt.
     * @param prompt_path Path/context payload used in the categorization prompt.
     * @return Retrieved whitelist candidate prompt section.
     */
    std::string build_large_whitelist_candidate_context(const std::string& prompt_name,
                                                        const std::string& prompt_path) const;
    /**
     * @brief Builds a prompt block with relevant user-learned category candidates.
     * @param prompt_name Name used in the categorization prompt.
     * @param prompt_path Path/context payload used in the categorization prompt.
     * @param file_type File or directory being categorized.
     * @return Learned candidate prompt section.
     */
    std::string build_learned_candidate_context(const std::string& prompt_name,
                                                const std::string& prompt_path,
                                                FileType file_type) const;
    /**
     * @brief Prefer a strong user-learned candidate over generic model output.
     * @param resolved Model-resolved category/subcategory before learned preference.
     * @param prompt_name Name used in the categorization prompt.
     * @param prompt_path Path/context payload used in the categorization prompt.
     * @param file_type File or directory being categorized.
     * @return Original or learned-preferred resolved category.
     */
    DatabaseManager::ResolvedCategory prefer_learned_candidate_for_generic_result(
        const DatabaseManager::ResolvedCategory& resolved,
        const std::string& prompt_name,
        const std::string& prompt_path,
        FileType file_type) const;
    /**
     * @brief Builds a prompt instruction for non-English category languages.
     * @return Language instruction block or empty string.
     */
    std::string build_category_language_context() const;

    /**
     * @brief Collects recent category assignments to provide consistency hints.
     * @param signature Signature key for the file type/extension.
     * @param session_history In-memory history of assignments.
     * @param extension File extension.
     * @param file_type File or directory.
     * @return List of up to kMaxConsistencyHints pairs.
     */
    std::vector<CategoryPair> collect_consistency_hints(
        const std::string& signature,
        const SessionHistoryMap& session_history,
        const std::string& extension,
        FileType file_type) const;

    /**
     * @brief Returns a cached categorization if it is valid for the entry.
     * @param item_name Display name for the item.
     * @param current_path Display path for the current on-disk location.
     * @param categorization_path Effective path used for categorization context.
     * @param dir_path Full directory path for cache lookup.
     * @param file_type File or directory.
     * @param progress_callback Progress updates callback.
     * @return Resolved category when cache is valid.
     */
    std::optional<DatabaseManager::ResolvedCategory> try_cached_categorization(
        const std::string& item_name,
        const std::string& current_path,
        const std::string& categorization_path,
        const std::string& dir_path,
        FileType file_type,
        const ProgressCallback& progress_callback) const;

    /**
     * @brief Ensures remote credentials are present and reports errors via progress callback.
     * @param item_name Display name for the item.
     * @param progress_callback Progress updates callback.
     * @return True when credentials are present or not required.
     */
    bool ensure_remote_credentials_for_request(
        const std::string& item_name,
        const ProgressCallback& progress_callback) const;

    /**
     * @brief Categorizes a single item by calling the LLM and validating the response.
     * @param llm LLM client used for the request.
     * @param is_local_llm True when using a local LLM backend.
     * @param display_name Display name for logging.
     * @param display_path Display path for logging.
     * @param prompt_name Name used in the prompt.
     * @param prompt_path Path used in the prompt.
     * @param file_type File or directory.
     * @param progress_callback Progress updates callback.
     * @param consistency_context Consistency hints block.
     * @return Resolved category for the item.
     */
    DatabaseManager::ResolvedCategory categorize_via_llm(
        ILLMClient& llm,
        bool is_local_llm,
        const std::string& display_name,
        const std::string& display_path,
        const std::string& prompt_name,
        const std::string& prompt_path,
        FileType file_type,
        const ProgressCallback& progress_callback,
        const std::string& consistency_context) const;

    /**
     * @brief Emits a formatted progress message for a categorization event.
     * @param progress_callback Progress updates callback.
     * @param source Label for the progress source.
     * @param item_name Display name for the item.
     * @param resolved Resolved category data.
     * @param current_path Display path for the current on-disk location.
     * @param categorization_path Effective path used for categorization context.
     */
    void emit_progress_message(const ProgressCallback& progress_callback,
                               std::string_view source,
                               const std::string& item_name,
                               const DatabaseManager::ResolvedCategory& resolved,
                               const std::string& current_path,
                               const std::string& categorization_path) const;

    /**
     * @brief Builds a signature key for consistency hints.
     * @param file_type File or directory.
     * @param extension File extension.
     * @return Signature key for consistency lookup.
     */
    static std::string make_file_signature(FileType file_type, const std::string& extension);
    /**
     * @brief Extracts a lowercase file extension (including the dot).
     * @param file_name File name to inspect.
     * @return Lowercase extension with dot, or empty string when none exists.
     */
    static std::string extract_extension(const std::string& file_name);
    /**
     * @brief Appends a unique, sanitized hint to the target list.
     * @param target Hint list to update.
     * @param candidate Candidate pair to append.
     * @return True when the hint was added.
     */
    static bool append_unique_hint(std::vector<CategoryPair>& target, const CategoryPair& candidate);
    /**
     * @brief Updates in-memory hint history with the latest assignment.
     * @param history Hint history to update.
     * @param assignment Category/subcategory assignment to record.
     */
    static void record_session_assignment(HintHistory& history, const CategoryPair& assignment);
    /**
     * @brief Formats consistency hints into a prompt block.
     * @param hints Consistency hints to format.
     * @return Prompt block string.
     */
    std::string format_hint_block(const std::vector<CategoryPair>& hints) const;

    /**
     * @brief Builds the per-entry inputs used by both the sequential and concurrent paths.
     * @param entry File entry to categorize.
     * @param prompt_override Optional prompt override.
     * @param suggested_name Suggested rename value.
     * @param session_history Session history snapshot used for consistency hints.
     * @return Prepared entry.
     */
    PreparedEntry prepare_entry(const FileEntry& entry,
                                const std::optional<PromptOverride>& prompt_override,
                                const std::string& suggested_name,
                                const SessionHistoryMap& session_history) const;

    /**
     * @brief Applies a resolved category: empty-result handling, persistence, and the review row.
     * @param prepared Prepared entry.
     * @param resolved Resolved category data.
     * @param is_local_llm True when using a local LLM backend.
     * @param recategorization_callback Callback for re-categorization events.
     * @param session_history Session history updated with the committed assignment.
     * @return Categorized entry when it should appear in the review list.
     */
    std::optional<CategorizedFile> finish_entry(const PreparedEntry& prepared,
                                                const DatabaseManager::ResolvedCategory& resolved,
                                                bool is_local_llm,
                                                const RecategorizationCallback& recategorization_callback,
                                                SessionHistoryMap& session_history) const;

    /**
     * @brief Sends the LLM request for a prepared entry. Safe to run on a worker thread.
     *
     * Only reads immutable settings and touches the supplied client. The request is synchronous
     * and relies on the client's transport timeout. Rate-limit waits are
     * performed here and reported through LlmResponse::notes instead of callbacks.
     * @param llm Client owned by the calling worker.
     * @param is_local_llm True when using a local LLM backend.
     * @param prepared Prepared entry.
     * @param stop_flag Cancellation flag.
     * @return Raw response or cancellation status.
     */
    LlmResponse execute_llm_request(ILLMClient& llm,
                                    bool is_local_llm,
                                    const PreparedEntry& prepared,
                                    std::atomic<bool>& stop_flag) const;

    /**
     * @brief Normalizes parsed LLM labels, resolves them and applies the whitelist.
     * @param display_name Display name for logging.
     * @param prompt_name Name used in the prompt.
     * @param prompt_path Path used in the prompt.
     * @param file_type File or directory.
     * @param category Category parsed from the LLM reply.
     * @param subcategory Subcategory parsed from the LLM reply.
     * @return Resolved category after normalization and whitelist handling.
     */
    DatabaseManager::ResolvedCategory resolve_llm_labels(const std::string& display_name,
                                                         const std::string& prompt_name,
                                                         const std::string& prompt_path,
                                                         FileType file_type,
                                                         std::string category,
                                                         std::string subcategory) const;

    /**
     * @brief Validates resolved labels and reports an invalid result to the progress callback and log.
     * @param resolved Resolved category to validate.
     * @param display_name Display name for logging.
     * @param progress_callback Progress updates callback.
     * @return True when the labels are invalid.
     */
    bool report_invalid_labels(const DatabaseManager::ResolvedCategory& resolved,
                               const std::string& display_name,
                               const ProgressCallback& progress_callback) const;

    /**
     * @brief Turns a raw LLM reply into a resolved category. Coordinator thread only.
     * @param llm Client used for optional localization.
     * @param display_name Display name for logging.
     * @param display_path Display path for logging.
     * @param prompt_name Name used in the prompt.
     * @param prompt_path Path used in the prompt.
     * @param file_type File or directory.
     * @param progress_callback Progress updates callback.
     * @param raw_response Raw LLM reply.
     * @return Resolved category; taxonomy_id -1 marks an invalid reply.
     */
    DatabaseManager::ResolvedCategory resolve_llm_category_response(
        ILLMClient& llm,
        const std::string& display_name,
        const std::string& display_path,
        const std::string& prompt_name,
        const std::string& prompt_path,
        FileType file_type,
        const ProgressCallback& progress_callback,
        const std::string& raw_response) const;

    /**
     * @brief Applies the image, document and artifact label normalizers and logs each change.
     * @param display_name Display name for logging.
     * @param prompt_name Name used in the prompt.
     * @param file_type File or directory.
     * @param category Category returned by the LLM.
     * @param subcategory Subcategory returned by the LLM.
     * @param effective_allowed_categories Whitelist main categories in effect.
     * @return Normalized category and subcategory.
     */
    std::pair<std::string, std::string> normalize_llm_labels(
        const std::string& display_name,
        const std::string& prompt_name,
        FileType file_type,
        std::string category,
        std::string subcategory,
        const std::vector<std::string>& effective_allowed_categories) const;

    /**
     * @brief Moves a resolved category onto the whitelist when it falls outside it.
     * @param resolved Resolved category, updated in place when the whitelist applies.
     * @param effective_allowed_categories Whitelist main categories in effect.
     * @param allowed_subcategories Flat whitelist subcategories.
     * @param allowed_subcategories_by_category Whitelist subcategories keyed by main category.
     */
    void apply_whitelist_to_resolved(
        DatabaseManager::ResolvedCategory& resolved,
        const std::vector<std::string>& effective_allowed_categories,
        const std::vector<std::string>& allowed_subcategories,
        const std::unordered_map<std::string, std::vector<std::string>>& allowed_subcategories_by_category) const;

    /**
     * @brief Emits and logs an LLM failure before it propagates.
     */
    void report_llm_failure(const ProgressCallback& progress_callback,
                            const std::string& display_name,
                            const std::exception& ex) const;

    /**
     * @brief Builds a rate-limit throttle that spaces out remote requests.
     * @param remote_requests_per_minute Limit; non-positive returns an empty callback.
     * @param progress_callback Progress updates callback (copied into the throttle).
     * @param stop_flag Cancellation flag that interrupts waits.
     * @return Throttle callback, or empty when throttling is disabled.
     */
    RemoteThrottleCallback make_remote_throttle(int remote_requests_per_minute,
                                                const ProgressCallback& progress_callback,
                                                std::atomic<bool>& stop_flag) const;

    /**
     * @brief Categorizes entries with bounded concurrent LLM requests.
     *
     * Entries are processed in chunks. For each chunk the coordinator prepares every entry in
     * input order, workers run the HTTP requests (at most `concurrency` at a time, each with its
     * own client), and the coordinator then commits results in input order.
     * @param files Entries to categorize.
     * @param is_local_llm True when using a local LLM backend.
     * @param concurrency Number of worker clients; must be greater than one.
     * @param stop_flag Cancellation flag.
     * @param llm_factory Creates one LLM client per worker slot.
     * @param callbacks Callbacks for progress, queueing, completion, recategorization, prompt
     *        overrides, suggested names and results.
     * @return Categorized entries, in input order.
     */
    std::vector<CategorizedFile> categorize_entries_concurrent(
        const std::vector<FileEntry>& files,
        bool is_local_llm,
        size_t concurrency,
        std::atomic<bool>& stop_flag,
        const std::function<std::unique_ptr<ILLMClient>()>& llm_factory,
        const EntryCallbacks& callbacks) const;

    /**
     * @brief Logs the summary line of a concurrent categorization run.
     * @param completed_items Number of items that counted toward completion.
     * @param started_at Time the run started.
     * @param concurrency Number of worker clients used.
     */
    void log_concurrent_run(size_t completed_items,
                            std::chrono::steady_clock::time_point started_at,
                            size_t concurrency) const;

    /**
     * @brief Creates one LLM client per worker slot on the calling (coordinator) thread.
     * @param llm_factory Creates one LLM client.
     * @param count Number of clients to create.
     * @return Created clients, in slot order.
     */
    std::vector<std::unique_ptr<ILLMClient>> create_worker_clients(
        const std::function<std::unique_ptr<ILLMClient>()>& llm_factory,
        size_t count) const;

    /**
     * @brief Prepares the entries of one chunk in input order and collects their LLM requests.
     * @param files All entries of the run.
     * @param chunk_begin Index of the first entry in the chunk.
     * @param chunk_end Index one past the last entry in the chunk.
     * @param is_local_llm True when using a local LLM backend.
     * @param stop_flag Cancellation flag; preparation stops early when set.
     * @param callbacks Callbacks for queueing, progress, prompt overrides and suggested names.
     * @param remote_throttle_callback Optional callback invoked before remote cache misses.
     * @param session_history Session history snapshot used for consistency hints.
     * @return Chunk items, and the requests they refer to by index.
     */
    ConcurrentChunk prepare_chunk(const std::vector<FileEntry>& files,
                                  size_t chunk_begin,
                                  size_t chunk_end,
                                  bool is_local_llm,
                                  std::atomic<bool>& stop_flag,
                                  const EntryCallbacks& callbacks,
                                  const RemoteThrottleCallback& remote_throttle_callback,
                                  const SessionHistoryMap& session_history) const;

    /**
     * @brief Prepares one entry of a concurrent chunk and decides whether it needs an LLM request.
     * @param entry File entry to prepare.
     * @param is_local_llm True when using a local LLM backend.
     * @param callbacks Callbacks used for suggested names, prompt overrides and progress.
     * @param remote_throttle_callback Optional callback invoked before remote cache misses.
     * @param session_history Session history snapshot used for consistency hints.
     * @return Chunk item; needs_request is set when a request must be dispatched.
     */
    ConcurrentChunkItem prepare_chunk_item(const FileEntry& entry,
                                           bool is_local_llm,
                                           const EntryCallbacks& callbacks,
                                           const RemoteThrottleCallback& remote_throttle_callback,
                                           const SessionHistoryMap& session_history) const;

    /**
     * @brief Commits one prepared chunk item in input order. Coordinator thread only.
     * @param item Chunk item; its resolved category is updated when a reply was received.
     * @param outcome Worker outcome for the item's request, or nullptr when it has none.
     * @param coordinator_llm Client used for localization.
     * @param is_local_llm True when using a local LLM backend.
     * @param callbacks Callbacks for progress, completion, recategorization and results.
     * @param session_history Session history updated with the committed assignment.
     * @param categorized Review list that committed entries are appended to.
     * @return True when the item counts toward completed items; false when it was never attempted.
     */
    bool commit_chunk_item(ConcurrentChunkItem& item,
                           BoundedWorkExecutor::Outcome<LlmResponse>* outcome,
                           ILLMClient& coordinator_llm,
                           bool is_local_llm,
                           const EntryCallbacks& callbacks,
                           SessionHistoryMap& session_history,
                           std::vector<CategorizedFile>& categorized) const;

    /**
     * @brief Applies a worker outcome to a chunk item. Coordinator thread only.
     *
     * Rethrows request failures after reporting them, replays rate-limit notes, and resolves the
     * reply unless it was cancelled.
     * @param item Chunk item; its resolved category is updated when a reply was received.
     * @param outcome Worker outcome for the item's request.
     * @param coordinator_llm Client used for localization.
     * @param progress Progress updates callback.
     * @return True when the reply was cancelled.
     */
    bool apply_request_outcome(ConcurrentChunkItem& item,
                               BoundedWorkExecutor::Outcome<LlmResponse>& outcome,
                               ILLMClient& coordinator_llm,
                               const ProgressCallback& progress) const;

    /**
     * @brief Finishes a committed chunk item and appends it to the review list. Coordinator thread only.
     * @param item Chunk item with its resolved category.
     * @param is_local_llm True when using a local LLM backend.
     * @param callbacks Callbacks for recategorization and results.
     * @param session_history Session history updated with the committed assignment.
     * @param categorized Review list that the entry is appended to when it should be shown.
     */
    void record_finished_entry(const ConcurrentChunkItem& item,
                               bool is_local_llm,
                               const EntryCallbacks& callbacks,
                               SessionHistoryMap& session_history,
                               std::vector<CategorizedFile>& categorized) const;

#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class CategorizationServiceTestAccess;
#endif

    Settings& settings;
    DatabaseManager& db_manager;
    std::shared_ptr<spdlog::logger> core_logger;
    UserLearningStore* user_learning_store_{nullptr};
};

#endif
