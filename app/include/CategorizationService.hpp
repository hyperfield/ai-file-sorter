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

class CategorizationService {
public:
    using ProgressCallback = std::function<void(const std::string&)>;
    using QueueCallback = std::function<void(const FileEntry&)>;
    using RecategorizationCallback = std::function<void(const CategorizedFile&, const std::string&)>;

    CategorizationService(Settings& settings,
                          DatabaseManager& db_manager,
                          std::shared_ptr<spdlog::logger> core_logger);

    bool ensure_remote_credentials(std::string* error_message = nullptr) const;
    std::vector<CategorizedFile> prune_empty_cached_entries(const std::string& directory_path);
    std::vector<CategorizedFile> load_cached_entries(const std::string& directory_path) const;

    std::vector<CategorizedFile> categorize_entries(
        const std::vector<FileEntry>& files,
        bool is_local_llm,
        std::atomic<bool>& stop_flag,
        const ProgressCallback& progress_callback,
        const QueueCallback& queue_callback,
        const RecategorizationCallback& recategorization_callback,
        std::function<std::unique_ptr<ILLMClient>()> llm_factory) const;

private:
    using CategoryPair = std::pair<std::string, std::string>;
    using HintHistory = std::deque<CategoryPair>;
    using SessionHistoryMap = std::unordered_map<std::string, HintHistory>;

    DatabaseManager::ResolvedCategory categorize_with_cache(
        ILLMClient& llm,
        bool is_local_llm,
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        const ProgressCallback& progress_callback,
        const std::string& consistency_context) const;

    std::optional<CategorizedFile> categorize_single_entry(
        ILLMClient& llm,
        bool is_local_llm,
        const FileEntry& entry,
        std::atomic<bool>& stop_flag,
        const ProgressCallback& progress_callback,
    const RecategorizationCallback& recategorization_callback,
    SessionHistoryMap& session_history) const;

    std::string build_combined_context(const std::string& hint_block) const;
    DatabaseManager::ResolvedCategory run_categorization_with_cache(
        ILLMClient& llm,
        bool is_local_llm,
        const FileEntry& entry,
        const ProgressCallback& progress_callback,
        const std::string& combined_context) const;
    std::optional<CategorizedFile> handle_empty_result(
        const FileEntry& entry,
        const std::string& dir_path,
        const DatabaseManager::ResolvedCategory& resolved,
        bool used_consistency_hints,
        bool is_local_llm,
        const RecategorizationCallback& recategorization_callback) const;
    void update_storage_with_result(const FileEntry& entry,
                                    const std::string& dir_path,
                                    const DatabaseManager::ResolvedCategory& resolved,
                                    bool used_consistency_hints,
                                    SessionHistoryMap& session_history) const;

    std::string run_llm_with_timeout(
        ILLMClient& llm,
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        bool is_local_llm,
        const std::string& consistency_context) const;
    int resolve_llm_timeout(bool is_local_llm) const;
    std::future<std::string> start_llm_future(ILLMClient& llm,
                                              const std::string& item_name,
                                              const std::string& item_path,
                                              FileType file_type,
                                              const std::string& consistency_context) const;

    std::string build_whitelist_context() const;
    std::string build_category_language_context() const;

    std::vector<CategoryPair> collect_consistency_hints(
        const std::string& signature,
        const SessionHistoryMap& session_history,
        const std::string& extension,
        FileType file_type) const;

    std::optional<DatabaseManager::ResolvedCategory> try_cached_categorization(
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        const ProgressCallback& progress_callback) const;

    bool ensure_remote_credentials_for_request(
        const std::string& item_name,
        const ProgressCallback& progress_callback) const;

    DatabaseManager::ResolvedCategory categorize_via_llm(
        ILLMClient& llm,
        bool is_local_llm,
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        const ProgressCallback& progress_callback,
        const std::string& consistency_context) const;

    void emit_progress_message(const ProgressCallback& progress_callback,
                               std::string_view source,
                               const std::string& item_name,
                               const DatabaseManager::ResolvedCategory& resolved,
                               const std::string& item_path) const;

    static std::string make_file_signature(FileType file_type, const std::string& extension);
    static std::string extract_extension(const std::string& file_name);
    static bool append_unique_hint(std::vector<CategoryPair>& target, const CategoryPair& candidate);
    static void record_session_assignment(HintHistory& history, const CategoryPair& assignment);
    std::string format_hint_block(const std::vector<CategoryPair>& hints) const;

#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class CategorizationServiceTestAccess;
#endif

    Settings& settings;
    DatabaseManager& db_manager;
    std::shared_ptr<spdlog::logger> core_logger;
};

#endif
