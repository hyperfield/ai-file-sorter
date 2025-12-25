#include "CategorizationService.hpp"

#include "Settings.hpp"
#include "CategoryLanguage.hpp"
#include "DatabaseManager.hpp"
#include "ILLMClient.hpp"
#include "Utils.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

namespace {
constexpr const char* kLocalTimeoutEnv = "AI_FILE_SORTER_LOCAL_LLM_TIMEOUT";
constexpr const char* kRemoteTimeoutEnv = "AI_FILE_SORTER_REMOTE_LLM_TIMEOUT";
constexpr size_t kMaxConsistencyHints = 5;
constexpr size_t kMaxLabelLength = 80;

std::pair<std::string, std::string> split_category_subcategory(const std::string& input) {
    const std::string delimiter = " : ";

    const auto pos = input.find(delimiter);
    if (pos == std::string::npos) {
        return {Utils::sanitize_path_label(input), ""};
    }

    auto category = input.substr(0, pos);
    auto subcategory = input.substr(pos + delimiter.size());
    return {Utils::sanitize_path_label(category), Utils::sanitize_path_label(subcategory)};
}

std::string to_lower_copy_str(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool contains_only_allowed_chars(const std::string& value) {
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        static const std::string forbidden = R"(<>:"/\|?*)";
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
        // Everything else is allowed (including non-ASCII letters and punctuation).
    }
    return true;
}

bool has_leading_or_trailing_space_or_dot(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    const unsigned char last = static_cast<unsigned char>(value.back());
    // Only guard leading/trailing whitespace; dots are allowed.
    return std::isspace(first) || std::isspace(last);
}

bool is_reserved_windows_name(const std::string& value) {
    static const std::vector<std::string> reserved = {
        "con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"
    };
    const std::string lower = to_lower_copy_str(value);
    return std::find(reserved.begin(), reserved.end(), lower) != reserved.end();
}

bool looks_like_extension_label(const std::string& value) {
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == value.size() - 1) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    if (ext.empty() || ext.size() > 5) {
        return false;
    }
    return std::all_of(ext.begin(), ext.end(), [](unsigned char ch) { return std::isalpha(ch); });
}

struct LabelValidationResult {
    bool valid{false};
    std::string error;
};

LabelValidationResult validate_labels(const std::string& category, const std::string& subcategory) {
    if (category.empty() || subcategory.empty()) {
        return {false, "Category or subcategory is empty"};
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        return {false, "Category or subcategory exceeds max length"};
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        return {false, "Category or subcategory contains disallowed characters"};
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        return {false, "Category or subcategory looks like a file extension"};
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        return {false, "Category or subcategory is a reserved name"};
    }
    if (has_leading_or_trailing_space_or_dot(category) || has_leading_or_trailing_space_or_dot(subcategory)) {
        return {false, "Category or subcategory has leading/trailing space or dot"};
    }
    if (to_lower_copy_str(category) == to_lower_copy_str(subcategory)) {
        return {false, "Category and subcategory are identical"};
    }
    return {true, {}};
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
    if (settings.get_llm_choice() == LLMChoice::Remote) {
        if (!settings.get_remote_api_key().empty()) {
            return true;
        }
        if (core_logger) {
            core_logger->error("Remote LLM selected but OpenAI API key is not configured.");
        }
        if (error_message) {
            *error_message = "Remote model credentials are missing. Enter your OpenAI API key in the Select LLM dialog.";
        }
        return false;
    }

    if (settings.get_llm_choice() == LLMChoice::Gemini) {
        if (!settings.get_gemini_api_key().empty()) {
            return true;
        }
        if (core_logger) {
            core_logger->error("Gemini LLM selected but Gemini API key is not configured.");
        }
        if (error_message) {
            *error_message = "Gemini API key is missing. Enter your Gemini API key in the Select LLM dialog.";
        }
        return false;
    }

    return true;
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
    SessionHistoryMap session_history;

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
                                                             recategorization_callback,
                                                             session_history)) {
            categorized.push_back(*categorized_entry);
        }
    }

    return categorized;
}

std::string CategorizationService::build_whitelist_context() const
{
    std::ostringstream oss;
    const auto cats = settings.get_allowed_categories();
    const auto subs = settings.get_allowed_subcategories();
    if (!cats.empty()) {
        oss << "Allowed main categories (pick exactly one label from the numbered list):\n";
        for (size_t i = 0; i < cats.size(); ++i) {
            oss << (i + 1) << ") " << cats[i] << "\n";
        }
    }
    if (!subs.empty()) {
        oss << "Allowed subcategories (pick exactly one label from the numbered list):\n";
        for (size_t i = 0; i < subs.size(); ++i) {
            oss << (i + 1) << ") " << subs[i] << "\n";
        }
    } else {
        oss << "Allowed subcategories: any (pick a specific, relevant subcategory; do not repeat the main category).";
    }
    return oss.str();
}

std::string CategorizationService::build_category_language_context() const
{
    const CategoryLanguage lang = settings.get_category_language();
    if (lang == CategoryLanguage::English) {
        return std::string();
    }
    const std::string name = categoryLanguageDisplay(lang);
    return fmt::format("Use {} for both the main category and subcategory names. Respond in {}.", name, name);
}

namespace {
bool is_allowed(const std::string& value, const std::vector<std::string>& allowed) {
    if (allowed.empty()) {
        return true;
    }
    const std::string norm = to_lower_copy_str(value);
    for (const auto& item : allowed) {
        if (to_lower_copy_str(item) == norm) {
            return true;
        }
    }
    return false;
}

std::string first_allowed_or_blank(const std::vector<std::string>& allowed) {
    return allowed.empty() ? std::string() : allowed.front();
}
} // namespace

std::optional<DatabaseManager::ResolvedCategory> CategorizationService::try_cached_categorization(
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    const ProgressCallback& progress_callback) const
{
    const auto cached = db_manager.get_categorization_from_db(item_name, file_type);
    if (cached.size() < 2) {
        return std::nullopt;
    }

    const std::string sanitized_category = Utils::sanitize_path_label(cached[0]);
    const std::string sanitized_subcategory = Utils::sanitize_path_label(cached[1]);
    if (sanitized_category.empty() || sanitized_subcategory.empty()) {
        if (core_logger) {
            core_logger->warn("Ignoring cached categorization with empty values for '{}'", item_name);
        }
        return std::nullopt;
    }
    const auto validation = validate_labels(sanitized_category, sanitized_subcategory);
    if (!validation.valid) {
        if (core_logger) {
            core_logger->warn("Ignoring cached categorization for '{}' due to validation error: {} (cat='{}', sub='{}')",
                              item_name,
                              validation.error,
                              sanitized_category,
                              sanitized_subcategory);
        }
        return std::nullopt;
    }

    auto resolved = db_manager.resolve_category(sanitized_category, sanitized_subcategory);
    emit_progress_message(progress_callback, "CACHE", item_name, resolved, item_path);
    return resolved;
}

bool CategorizationService::ensure_remote_credentials_for_request(
    const std::string& item_name,
    const ProgressCallback& progress_callback) const
{
    if (!settings.get_remote_api_key().empty()) {
        return true;
    }

    const std::string err_msg = fmt::format("[REMOTE] {} (missing OpenAI API key)", item_name);
    if (progress_callback) {
        progress_callback(err_msg);
    }
    if (core_logger) {
        core_logger->error("{}", err_msg);
    }
    return false;
}

DatabaseManager::ResolvedCategory CategorizationService::categorize_via_llm(
    ILLMClient& llm,
    bool is_local_llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    const ProgressCallback& progress_callback,
    const std::string& consistency_context) const
{
    try {
        const std::string category_subcategory =
            run_llm_with_timeout(llm, item_name, item_path, file_type, is_local_llm, consistency_context);

        auto [category, subcategory] = split_category_subcategory(category_subcategory);
        auto resolved = db_manager.resolve_category(category, subcategory);
        if (settings.get_use_whitelist()) {
            const auto allowed_categories = settings.get_allowed_categories();
            const auto allowed_subcategories = settings.get_allowed_subcategories();
            if (!is_allowed(resolved.category, allowed_categories)) {
                resolved.category = first_allowed_or_blank(allowed_categories);
            }
            if (!is_allowed(resolved.subcategory, allowed_subcategories)) {
                resolved.subcategory = first_allowed_or_blank(allowed_subcategories);
            }
        }
        const auto validation = validate_labels(resolved.category, resolved.subcategory);
        if (!validation.valid) {
            if (progress_callback) {
                progress_callback(fmt::format("[LLM-ERROR] {} (invalid category/subcategory: {})",
                                              item_name,
                                              validation.error));
            }
            if (core_logger) {
                core_logger->warn("Invalid LLM output for '{}': {} (cat='{}', sub='{}')",
                                  item_name,
                                  validation.error,
                                  resolved.category,
                                  resolved.subcategory);
            }
            return DatabaseManager::ResolvedCategory{-1, "", ""};
        }
        if (resolved.category.empty()) {
            resolved.category = "Uncategorized";
        }
        emit_progress_message(progress_callback, "AI", item_name, resolved, item_path);
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

void CategorizationService::emit_progress_message(const ProgressCallback& progress_callback,
                                                  std::string_view source,
                                                  const std::string& item_name,
                                                  const DatabaseManager::ResolvedCategory& resolved,
                                                  const std::string& item_path) const
{
    if (!progress_callback) {
        return;
    }
    const std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
    const std::string path_display = item_path.empty() ? "-" : item_path;

    progress_callback(fmt::format(
        "[{}] {}\n    Category : {}\n    Subcat   : {}\n    Path     : {}",
        source, item_name, resolved.category, sub, path_display));
}

DatabaseManager::ResolvedCategory CategorizationService::categorize_with_cache(
    ILLMClient& llm,
    bool is_local_llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    const ProgressCallback& progress_callback,
    const std::string& consistency_context) const
{
    if (auto cached = try_cached_categorization(item_name, item_path, file_type, progress_callback)) {
        return *cached;
    }

    if (!is_local_llm && !ensure_remote_credentials_for_request(item_name, progress_callback)) {
        return DatabaseManager::ResolvedCategory{-1, "", ""};
    }

    return categorize_via_llm(llm,
                              is_local_llm,
                              item_name,
                              item_path,
                              file_type,
                              progress_callback,
                              consistency_context);
}

std::optional<CategorizedFile> CategorizationService::categorize_single_entry(
    ILLMClient& llm,
    bool is_local_llm,
    const FileEntry& entry,
    std::atomic<bool>& stop_flag,
    const ProgressCallback& progress_callback,
    const RecategorizationCallback& recategorization_callback,
    SessionHistoryMap& session_history) const
{
    (void)stop_flag;

    const std::filesystem::path entry_path = Utils::utf8_to_path(entry.full_path);
    const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
    const std::string abbreviated_path = Utils::abbreviate_user_path(entry.full_path);
    const bool use_consistency_hints = settings.get_use_consistency_hints();
    const std::string extension = extract_extension(entry.file_name);
    const std::string signature = make_file_signature(entry.type, extension);
    std::string hint_block;
    if (use_consistency_hints) {
        const auto hints = collect_consistency_hints(signature, session_history, extension, entry.type);
        hint_block = format_hint_block(hints);
    }
    const std::string combined_context = build_combined_context(hint_block);

    DatabaseManager::ResolvedCategory resolved =
        run_categorization_with_cache(llm,
                                      is_local_llm,
                                      entry,
                                      progress_callback,
                                      combined_context);

    if (auto retry = handle_empty_result(entry,
                                         dir_path,
                                         resolved,
                                         use_consistency_hints,
                                         is_local_llm,
                                         recategorization_callback)) {
        return retry;
    }

    update_storage_with_result(entry, dir_path, resolved, use_consistency_hints, session_history);

    CategorizedFile result{dir_path, entry.file_name, entry.type,
                           resolved.category, resolved.subcategory, resolved.taxonomy_id};
    result.used_consistency_hints = use_consistency_hints;
    return result;
}

std::string CategorizationService::build_combined_context(const std::string& hint_block) const
{
    std::string combined_context;
    const std::string whitelist_block = build_whitelist_context();
    const std::string language_block = build_category_language_context();
    if (!language_block.empty()) {
        combined_context += language_block;
    }
    if (settings.get_use_whitelist() && !whitelist_block.empty()) {
        if (core_logger) {
            core_logger->debug("Applying category whitelist ({} cats, {} subs)",
                               settings.get_allowed_categories().size(),
                               settings.get_allowed_subcategories().size());
        }
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += whitelist_block;
    }
    if (!hint_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += hint_block;
    }
    return combined_context;
}

DatabaseManager::ResolvedCategory CategorizationService::run_categorization_with_cache(
    ILLMClient& llm,
    bool is_local_llm,
    const FileEntry& entry,
    const ProgressCallback& progress_callback,
    const std::string& combined_context) const
{
    const std::string abbreviated_path = Utils::abbreviate_user_path(entry.full_path);
    return categorize_with_cache(llm,
                                 is_local_llm,
                                 entry.file_name,
                                 abbreviated_path,
                                 entry.type,
                                 progress_callback,
                                 combined_context);
}

std::optional<CategorizedFile> CategorizationService::handle_empty_result(
    const FileEntry& entry,
    const std::string& dir_path,
    const DatabaseManager::ResolvedCategory& resolved,
    bool used_consistency_hints,
    bool is_local_llm,
    const RecategorizationCallback& recategorization_callback) const
{
    const bool invalid = resolved.taxonomy_id == -1;
    if (!resolved.category.empty() && !resolved.subcategory.empty() && !invalid) {
        return std::nullopt;
    }

    const std::string reason = invalid
        ? "Categorization returned invalid category/subcategory and was skipped."
        : "Categorization returned no result.";

    if (core_logger) {
        core_logger->warn("{} for '{}'.", reason, entry.file_name);
    }

    db_manager.remove_file_categorization(dir_path, entry.file_name, entry.type);

    if (recategorization_callback) {
        CategorizedFile retry_entry{dir_path,
                                    entry.file_name,
                                    entry.type,
                                    resolved.category,
                                    resolved.subcategory,
                                    resolved.taxonomy_id};
        retry_entry.used_consistency_hints = used_consistency_hints;
        recategorization_callback(retry_entry, reason);
    }
    return std::nullopt;
}

void CategorizationService::update_storage_with_result(const FileEntry& entry,
                                                       const std::string& dir_path,
                                                       const DatabaseManager::ResolvedCategory& resolved,
                                                       bool used_consistency_hints,
                                                       SessionHistoryMap& session_history) const
{
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
        resolved,
        used_consistency_hints);

    const std::string signature = make_file_signature(entry.type, extract_extension(entry.file_name));
    if (!signature.empty()) {
        record_session_assignment(session_history[signature], {resolved.category, resolved.subcategory});
    }
}

std::string CategorizationService::run_llm_with_timeout(
    ILLMClient& llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    bool is_local_llm,
    const std::string& consistency_context) const
{
    const int timeout_seconds = resolve_llm_timeout(is_local_llm);

    auto future = start_llm_future(llm, item_name, item_path, file_type, consistency_context);

    if (future.wait_for(std::chrono::seconds(timeout_seconds)) == std::future_status::timeout) {
        throw std::runtime_error("Timed out waiting for LLM response");
    }

    return future.get();
}

int CategorizationService::resolve_llm_timeout(bool is_local_llm) const
{
    int timeout_seconds = is_local_llm ? 60 : 10;
    const char* timeout_env = std::getenv(is_local_llm ? kLocalTimeoutEnv : kRemoteTimeoutEnv);
    if (!timeout_env || *timeout_env == '\0') {
        return timeout_seconds;
    }

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

    if (core_logger) {
        core_logger->debug("Using {} LLM timeout of {} second(s)",
                           is_local_llm ? "local" : "remote",
                           timeout_seconds);
    }

    return timeout_seconds;
}

std::future<std::string> CategorizationService::start_llm_future(
    ILLMClient& llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    const std::string& consistency_context) const
{
    auto promise = std::make_shared<std::promise<std::string>>();
    std::future<std::string> future = promise->get_future();

    std::thread([&llm, promise, item_name, item_path, file_type, consistency_context]() mutable {
        try {
            promise->set_value(llm.categorize_file(item_name, item_path, file_type, consistency_context));
        } catch (...) {
            try {
                promise->set_exception(std::current_exception());
            } catch (...) {
                // no-op
            }
        }
    }).detach();

    return future;
}

std::vector<CategorizationService::CategoryPair> CategorizationService::collect_consistency_hints(
    const std::string& signature,
    const SessionHistoryMap& session_history,
    const std::string& extension,
    FileType file_type) const
{
    std::vector<CategoryPair> hints;
    if (signature.empty()) {
        return hints;
    }

    if (auto it = session_history.find(signature); it != session_history.end()) {
        for (const auto& entry : it->second) {
            if (append_unique_hint(hints, entry) && hints.size() == kMaxConsistencyHints) {
                return hints;
            }
        }
    }

    if (hints.size() < kMaxConsistencyHints) {
        const size_t remaining = kMaxConsistencyHints - hints.size();
        const auto db_hints = db_manager.get_recent_categories_for_extension(extension, file_type, remaining);
        for (const auto& entry : db_hints) {
            if (append_unique_hint(hints, entry) && hints.size() == kMaxConsistencyHints) {
                break;
            }
        }
    }

    return hints;
}

std::string CategorizationService::make_file_signature(FileType file_type, const std::string& extension)
{
    const std::string type_tag = (file_type == FileType::Directory) ? "DIR" : "FILE";
    const std::string normalized_extension = extension.empty() ? std::string("<none>") : extension;
    return type_tag + ":" + normalized_extension;
}

std::string CategorizationService::extract_extension(const std::string& file_name)
{
    const auto pos = file_name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= file_name.size()) {
        return std::string();
    }
    std::string ext = file_name.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

bool CategorizationService::append_unique_hint(std::vector<CategoryPair>& target, const CategoryPair& candidate)
{
    CategoryPair normalized{Utils::sanitize_path_label(candidate.first), Utils::sanitize_path_label(candidate.second)};
    if (normalized.first.empty()) {
        return false;
    }
    if (normalized.second.empty()) {
        normalized.second = normalized.first;
    }
    for (const auto& existing : target) {
        if (existing.first == normalized.first && existing.second == normalized.second) {
            return false;
        }
    }
    target.push_back(std::move(normalized));
    return true;
}

void CategorizationService::record_session_assignment(HintHistory& history, const CategoryPair& assignment)
{
    CategoryPair normalized{Utils::sanitize_path_label(assignment.first), Utils::sanitize_path_label(assignment.second)};
    if (normalized.first.empty()) {
        return;
    }
    if (normalized.second.empty()) {
        normalized.second = normalized.first;
    }

    history.erase(std::remove(history.begin(), history.end(), normalized), history.end());
    history.push_front(normalized);
    if (history.size() > kMaxConsistencyHints) {
        history.pop_back();
    }
}

std::string CategorizationService::format_hint_block(const std::vector<CategoryPair>& hints) const
{
    if (hints.empty()) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "Recent assignments for similar items:\n";
    for (const auto& hint : hints) {
        const std::string sub = hint.second.empty() ? hint.first : hint.second;
        oss << "- " << hint.first << " : " << sub << "\n";
    }
    oss << "Prefer one of the above when it fits; otherwise, choose the closest consistent alternative.";
    return oss.str();
}
