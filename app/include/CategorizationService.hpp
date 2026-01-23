#ifndef CATEGORIZATION_SERVICE_HPP
#define CATEGORIZATION_SERVICE_HPP

#include "Types.hpp"
#include "DatabaseManager.hpp"

#include <atomic>
#include <deque>
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
namespace spdlog { class logger; }

/**
 * @brief Provides LLM-backed file categorization with caching and validation.
 */
class CategorizationService {
public:
    using ProgressCallback = std::function<void(const std::string&)>;
    using QueueCallback = std::function<void(const FileEntry&)>;
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
                          std::shared_ptr<spdlog::logger> core_logger);

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
        const RecategorizationCallback& recategorization_callback,
        std::function<std::unique_ptr<ILLMClient>()> llm_factory,
        const PromptOverrideProvider& prompt_override = {},
        const SuggestedNameProvider& suggested_name_provider = {}) const;

private:
    using CategoryPair = std::pair<std::string, std::string>;
    using HintHistory = std::deque<CategoryPair>;
    using SessionHistoryMap = std::unordered_map<std::string, HintHistory>;

    /**
     * @brief Returns a cached categorization when available, otherwise calls the LLM.
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
    DatabaseManager::ResolvedCategory categorize_with_cache(
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
        SessionHistoryMap& session_history) const;

    /**
     * @brief Combines language, whitelist, and hint blocks into a single prompt context.
     * @param hint_block Consistency hint block.
     * @return Combined prompt context.
     */
    std::string build_combined_context(const std::string& hint_block) const;
    /**
     * @brief Runs the categorization flow with cache handling for a single entry.
     * @param llm LLM client used for the request.
     * @param is_local_llm True when using a local LLM backend.
     * @param entry File entry to categorize.
     * @param display_path Display path for logging.
     * @param prompt_name Name used in the prompt.
     * @param prompt_path Path used in the prompt.
     * @param progress_callback Progress updates callback.
     * @param combined_context Combined prompt context.
     * @return Resolved category for the item.
     */
    DatabaseManager::ResolvedCategory run_categorization_with_cache(
        ILLMClient& llm,
        bool is_local_llm,
        const FileEntry& entry,
        const std::string& display_path,
        const std::string& prompt_name,
        const std::string& prompt_path,
        const ProgressCallback& progress_callback,
        const std::string& combined_context) const;
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
     * @param item_path Display path for the item.
     * @param file_type File or directory.
     * @param progress_callback Progress updates callback.
     * @return Resolved category when cache is valid.
     */
    std::optional<DatabaseManager::ResolvedCategory> try_cached_categorization(
        const std::string& item_name,
        const std::string& item_path,
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
     * @param item_path Display path for the item.
     */
    void emit_progress_message(const ProgressCallback& progress_callback,
                               std::string_view source,
                               const std::string& item_name,
                               const DatabaseManager::ResolvedCategory& resolved,
                               const std::string& item_path) const;

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

#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class CategorizationServiceTestAccess;
#endif

    Settings& settings;
    DatabaseManager& db_manager;
    std::shared_ptr<spdlog::logger> core_logger;
};

#endif
