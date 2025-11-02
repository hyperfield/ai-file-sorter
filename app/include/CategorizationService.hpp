#ifndef CATEGORIZATION_SERVICE_HPP
#define CATEGORIZATION_SERVICE_HPP

#include "Types.hpp"
#include "DatabaseManager.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
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
    DatabaseManager::ResolvedCategory categorize_with_cache(
        ILLMClient& llm,
        bool is_local_llm,
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        const ProgressCallback& progress_callback) const;

    std::optional<CategorizedFile> categorize_single_entry(
        ILLMClient& llm,
        bool is_local_llm,
        const FileEntry& entry,
        std::atomic<bool>& stop_flag,
        const ProgressCallback& progress_callback,
        const RecategorizationCallback& recategorization_callback) const;

    std::string run_llm_with_timeout(
        ILLMClient& llm,
        const std::string& item_name,
        const std::string& item_path,
        FileType file_type,
        bool is_local_llm) const;

    Settings& settings;
    DatabaseManager& db_manager;
    std::shared_ptr<spdlog::logger> core_logger;
};

#endif
