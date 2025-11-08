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
    DatabaseManager& db_manager;
    std::shared_ptr<spdlog::logger> logger;
    mutable bool prompt_logging_enabled{false};
};

#endif
