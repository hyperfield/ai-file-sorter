#ifndef CONSISTENCY_PASS_SERVICE_HPP
#define CONSISTENCY_PASS_SERVICE_HPP

#include "DatabaseManager.hpp"
#include "Types.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class ILLMClient;
namespace spdlog { class logger; }

class ConsistencyPassService {
public:
    using ProgressCallback = std::function<void(const std::string&)>;

    ConsistencyPassService(DatabaseManager& db_manager,
                           std::shared_ptr<spdlog::logger> logger);

    void set_prompt_logging_enabled(bool enabled);

    void run(std::vector<CategorizedFile>& categorized_files,
             std::vector<CategorizedFile>& newly_categorized_files,
             std::function<std::unique_ptr<ILLMClient>()> llm_factory,
             std::atomic<bool>& stop_flag,
             const ProgressCallback& progress_callback) const;

private:
    std::unique_ptr<ILLMClient> create_llm(std::function<std::unique_ptr<ILLMClient>()> llm_factory) const;
    void process_chunks(ILLMClient& llm,
                        const std::vector<std::pair<std::string, std::string>>& taxonomy,
                        std::vector<CategorizedFile>& categorized_files,
                        std::unordered_map<std::string, CategorizedFile*>& items_by_key,
                        std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
                        std::atomic<bool>& stop_flag,
                        const ProgressCallback& progress_callback) const;
    void process_chunk(const std::vector<const CategorizedFile*>& chunk,
                       size_t start_index,
                       size_t end_index,
                       size_t total_items,
                       ILLMClient& llm,
                       const std::vector<std::pair<std::string, std::string>>& taxonomy,
                       std::unordered_map<std::string, CategorizedFile*>& items_by_key,
                       std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
                       const ProgressCallback& progress_callback) const;
    void log_chunk_items(const std::vector<const CategorizedFile*>& chunk, const char* stage) const;
    bool apply_harmonized_response(const std::string& response,
                                   const std::vector<const CategorizedFile*>& chunk,
                                   std::unordered_map<std::string, CategorizedFile*>& items_by_key,
                                   std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
                                   const ProgressCallback& progress_callback,
                                   DatabaseManager& db_manager) const;

    DatabaseManager& db_manager;
    std::shared_ptr<spdlog::logger> logger;
    mutable bool prompt_logging_enabled{false};
};

#endif
