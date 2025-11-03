#include "CategorizationService.hpp"

#include "Settings.hpp"
#include "DatabaseManager.hpp"
#include "CryptoManager.hpp"
#include "ILLMClient.hpp"
#include "Utils.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <thread>

namespace {
constexpr const char* kLocalTimeoutEnv = "AI_FILE_SORTER_LOCAL_LLM_TIMEOUT";
constexpr const char* kRemoteTimeoutEnv = "AI_FILE_SORTER_REMOTE_LLM_TIMEOUT";

std::pair<std::string, std::string> split_category_subcategory(const std::string& input) {
    const std::string delimiter = " : ";

    const auto pos = input.find(delimiter);
    if (pos == std::string::npos) {
        return {input, ""};
    }

    auto category = input.substr(0, pos);
    auto subcategory = input.substr(pos + delimiter.size());
    return {category, subcategory};
}

std::string trim_whitespace(const std::string& value) {
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    const auto end = value.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) {
        return std::string();
    }
    return value.substr(start, end - start + 1);
}
}

CategorizationService::CategorizationService(Settings& settings,
                                             DatabaseManager& db_manager,
                                             std::shared_ptr<spdlog::logger> core_logger)
    : settings(settings),
      db_manager(db_manager),
      core_logger(std::move(core_logger)) {}

bool CategorizationService::ensure_remote_credentials(std::string* error_message) const
{
    if (settings.get_llm_choice() != LLMChoice::Remote) {
        return true;
    }

    const char* env_pc = std::getenv("ENV_PC");
    const char* env_rr = std::getenv("ENV_RR");

    try {
        CryptoManager crypto(env_pc, env_rr);
        const std::string key = crypto.reconstruct();
        if (key.empty()) {
            throw std::runtime_error("Reconstructed API key was empty");
        }
        return true;
    } catch (const std::exception& ex) {
        if (core_logger) {
            core_logger->error("Remote LLM credentials unavailable: {}", ex.what());
        }
        if (error_message) {
            *error_message = "Remote model credentials are missing or invalid. "
                             "Please configure your API key and try again.";
        }
        return false;
    }
}

std::vector<CategorizedFile> CategorizationService::prune_empty_cached_entries(const std::string& directory_path)
{
    return db_manager.remove_empty_categorizations(directory_path);
}

std::vector<CategorizedFile> CategorizationService::load_cached_entries(const std::string& directory_path) const
{
    return db_manager.get_categorized_files(directory_path);
}

std::vector<CategorizedFile> CategorizationService::categorize_entries(
    const std::vector<FileEntry>& files,
    bool is_local_llm,
    std::atomic<bool>& stop_flag,
    const ProgressCallback& progress_callback,
    const QueueCallback& queue_callback,
    const RecategorizationCallback& recategorization_callback,
    std::function<std::unique_ptr<ILLMClient>()> llm_factory) const
{
    std::vector<CategorizedFile> categorized;
    if (files.empty()) {
        return categorized;
    }

    auto llm = llm_factory ? llm_factory() : nullptr;
    if (!llm) {
        throw std::runtime_error("Failed to create LLM client.");
    }

    categorized.reserve(files.size());

    for (const auto& entry : files) {
        if (stop_flag.load()) {
            break;
        }

        if (queue_callback) {
            queue_callback(entry);
        }

        if (auto categorized_entry = categorize_single_entry(*llm,
                                                             is_local_llm,
                                                             entry,
                                                             stop_flag,
                                                             progress_callback,
                                                             recategorization_callback)) {
            categorized.push_back(*categorized_entry);
        }
    }

    return categorized;
}

DatabaseManager::ResolvedCategory CategorizationService::categorize_with_cache(
    ILLMClient& llm,
    bool is_local_llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    const ProgressCallback& progress_callback) const
{
    const auto cached = db_manager.get_categorization_from_db(item_name, file_type);
    if (cached.size() >= 2) {
        const std::string& cached_category = cached[0];
        const std::string& cached_subcategory = cached[1];

        const std::string trimmed_category = trim_whitespace(cached_category);
        const std::string trimmed_subcategory = trim_whitespace(cached_subcategory);

        if (trimmed_category.empty() || trimmed_subcategory.empty()) {
            if (core_logger) {
                core_logger->warn("Ignoring cached categorization with empty values for '{}'", item_name);
            }
        } else {
            auto resolved = db_manager.resolve_category(cached_category, cached_subcategory);
            const std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
            const std::string path_display = item_path.empty() ? "-" : item_path;

            if (progress_callback) {
                progress_callback(fmt::format(
                    "[CACHE] {}\n    Category : {}\n    Subcat   : {}\n    Path     : {}",
                    item_name, resolved.category, sub, path_display));
            }
            return resolved;
        }
    }

    if (!is_local_llm) {
        const char* env_pc = std::getenv("ENV_PC");
        const char* env_rr = std::getenv("ENV_RR");

        try {
            CryptoManager crypto(env_pc, env_rr);
            const std::string key = crypto.reconstruct();
            if (key.empty()) {
                throw std::runtime_error("Reconstructed API key was empty");
            }
        } catch (const std::exception& ex) {
            const std::string err_msg = fmt::format("[CRYPTO] {} ({})", item_name, ex.what());
            if (progress_callback) {
                progress_callback(err_msg);
            }
            if (core_logger) {
                core_logger->error("{}", err_msg);
            }
            return DatabaseManager::ResolvedCategory{-1, "", ""};
        }
    }

    try {
        const std::string category_subcategory =
            run_llm_with_timeout(llm, item_name, item_path, file_type, is_local_llm);

        auto [category, subcategory] = split_category_subcategory(category_subcategory);
        auto resolved = db_manager.resolve_category(category, subcategory);

        const std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
        const std::string path_display = item_path.empty() ? "-" : item_path;

        if (progress_callback) {
            progress_callback(fmt::format(
                "[AI] {}\n    Category : {}\n    Subcat   : {}\n    Path     : {}",
                item_name, resolved.category, sub, path_display));
        }

        return resolved;
    } catch (const std::exception& ex) {
        const std::string err_msg = fmt::format("[LLM-ERROR] {} ({})", item_name, ex.what());
        if (progress_callback) {
            progress_callback(err_msg);
        }
        if (core_logger) {
            core_logger->error("LLM error while categorizing '{}': {}", item_name, ex.what());
        }
        throw;
    }
}

std::optional<CategorizedFile> CategorizationService::categorize_single_entry(
    ILLMClient& llm,
    bool is_local_llm,
    const FileEntry& entry,
    std::atomic<bool>& stop_flag,
    const ProgressCallback& progress_callback,
    const RecategorizationCallback& recategorization_callback) const
{
    (void)stop_flag;

    const std::filesystem::path entry_path = Utils::utf8_to_path(entry.full_path);
    const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
    const std::string abbreviated_path = Utils::abbreviate_user_path(entry.full_path);

    DatabaseManager::ResolvedCategory resolved =
        categorize_with_cache(llm, is_local_llm, entry.file_name, abbreviated_path, entry.type, progress_callback);

    if (resolved.category.empty() || resolved.subcategory.empty()) {
        if (core_logger) {
            core_logger->warn("Categorization for '{}' returned empty category/subcategory.", entry.file_name);
        }
        db_manager.remove_file_categorization(dir_path, entry.file_name, entry.type);

        if (recategorization_callback) {
            const std::string reason = is_local_llm
                ? "Categorization returned no result. The item will be processed again."
                : "Categorization returned no result. Configure your remote API key and try again.";
            recategorization_callback(CategorizedFile{dir_path,
                                                      entry.file_name,
                                                      entry.type,
                                                      resolved.category,
                                                      resolved.subcategory,
                                                      resolved.taxonomy_id},
                                      reason);
        }
        return std::nullopt;
    }

    if (core_logger) {
        core_logger->info("Categorized '{}' as '{} / {}'.",
                          entry.file_name,
                          resolved.category,
                          resolved.subcategory.empty() ? "<none>" : resolved.subcategory);
    }

    db_manager.insert_or_update_file_with_categorization(
        entry.file_name,
        entry.type == FileType::File ? "F" : "D",
        dir_path,
        resolved);

    return CategorizedFile{dir_path, entry.file_name, entry.type,
                           resolved.category, resolved.subcategory, resolved.taxonomy_id};
}

std::string CategorizationService::run_llm_with_timeout(
    ILLMClient& llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    bool is_local_llm) const
{
    int timeout_seconds = is_local_llm ? 60 : 10;
    const char* timeout_env = std::getenv(is_local_llm ? kLocalTimeoutEnv : kRemoteTimeoutEnv);
    if (timeout_env && *timeout_env) {
        try {
            const int parsed = std::stoi(timeout_env);
            if (parsed > 0) {
                timeout_seconds = parsed;
            } else if (core_logger) {
                core_logger->warn("Ignoring non-positive LLM timeout '{}'", timeout_env);
            }
        } catch (const std::exception& ex) {
            if (core_logger) {
                core_logger->warn("Failed to parse LLM timeout '{}': {}", timeout_env, ex.what());
            }
        }
    }

    if (core_logger) {
        core_logger->debug("Using {} LLM timeout of {} second(s)",
                           is_local_llm ? "local" : "remote",
                           timeout_seconds);
    }

    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread([&llm, &promise, item_name, item_path, file_type]() mutable {
        try {
            promise.set_value(llm.categorize_file(item_name, item_path, file_type));
        } catch (...) {
            try {
                promise.set_exception(std::current_exception());
            } catch (...) {
                // no-op
            }
        }
    }).detach();

    if (future.wait_for(std::chrono::seconds(timeout_seconds)) == std::future_status::timeout) {
        throw std::runtime_error("Timed out waiting for LLM response");
    }

    return future.get();
}
