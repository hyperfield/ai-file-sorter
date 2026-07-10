#include "HeadlessReviewApplyService.hpp"

#include "DatabaseManager.hpp"
#include "MovableCategorizedFile.hpp"
#include "ReviewHistoryStore.hpp"
#include "StorageProvider.hpp"
#include "UndoManager.hpp"
#include "Utils.hpp"

#include <spdlog/logger.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool is_missing_category_label(const std::string& value)
{
    const std::string trimmed = trim_copy(value);
    return trimmed.empty() || to_lower_copy(trimmed) == "uncategorized";
}

std::string strip_history_description_label(std::string value)
{
    value = trim_copy(value);
    constexpr std::string_view image_prefix = "Image description: ";
    constexpr std::string_view document_prefix = "Document summary: ";
    if (value.starts_with(image_prefix)) {
        return trim_copy(value.substr(image_prefix.size()));
    }
    if (value.starts_with(document_prefix)) {
        return trim_copy(value.substr(document_prefix.size()));
    }
    return value;
}

bool contains_only_allowed_chars(const std::string& value)
{
    static const std::string forbidden = R"(<>:"/\|?*)";
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool is_reserved_windows_name(const std::string& value)
{
    static const std::vector<std::string> reserved = {
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
    return std::find(reserved.begin(), reserved.end(), to_lower_copy(value)) != reserved.end();
}

bool looks_like_extension_label(const std::string& value)
{
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos + 1 >= value.size()) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    return !ext.empty() && ext.size() <= 5 &&
           std::all_of(ext.begin(), ext.end(), [](unsigned char ch) { return std::isalpha(ch); });
}

bool has_leading_or_trailing_space_or_dot(const std::string& value)
{
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    const unsigned char last = static_cast<unsigned char>(value.back());
    return std::isspace(first) || std::isspace(last) ||
           value.front() == '.' || value.back() == '.';
}

bool validate_label_pair(const std::string& category,
                         const std::string& subcategory,
                         bool allow_identical,
                         std::string* error)
{
    constexpr std::size_t kMaxLabelLength = 80;
    if (category.empty() || subcategory.empty()) {
        if (error) {
            *error = "Category or subcategory is empty.";
        }
        return false;
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        if (error) {
            *error = "Category or subcategory exceeds max length.";
        }
        return false;
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        if (error) {
            *error = "Category or subcategory contains disallowed characters.";
        }
        return false;
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        if (error) {
            *error = "Category or subcategory looks like a file extension.";
        }
        return false;
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        if (error) {
            *error = "Category or subcategory is a reserved name.";
        }
        return false;
    }
    if (has_leading_or_trailing_space_or_dot(category) ||
        has_leading_or_trailing_space_or_dot(subcategory)) {
        if (error) {
            *error = "Category or subcategory has leading/trailing space or dot.";
        }
        return false;
    }
    if (!allow_identical && to_lower_copy(category) == to_lower_copy(subcategory)) {
        if (error) {
            *error = "Category and subcategory are identical.";
        }
        return false;
    }
    return true;
}

bool validate_filename(const std::string& name, std::string* error)
{
    if (name.empty() || name == "." || name == "..") {
        if (error) {
            *error = "Filename is invalid.";
        }
        return false;
    }
    if (!contains_only_allowed_chars(name)) {
        if (error) {
            *error = "Filename contains disallowed characters.";
        }
        return false;
    }
    if (has_leading_or_trailing_space_or_dot(name)) {
        if (error) {
            *error = "Filename has leading/trailing space or dot.";
        }
        return false;
    }

    const auto path = Utils::utf8_to_path(name);
    const std::string stem = Utils::path_to_utf8(path.stem());
    if (stem.empty()) {
        if (error) {
            *error = "Filename is missing a base name.";
        }
        return false;
    }
    if (is_reserved_windows_name(stem)) {
        if (error) {
            *error = "Filename is a reserved name.";
        }
        return false;
    }
    return true;
}

std::string resolve_destination_name(const CategorizedFile& entry, bool apply_suggested_name)
{
    if (!apply_suggested_name) {
        return entry.file_name;
    }

    const std::string trimmed = trim_copy(entry.suggested_name);
    if (trimmed.empty() || trimmed == entry.file_name) {
        return entry.file_name;
    }

    const std::filesystem::path original_path = Utils::utf8_to_path(entry.file_name);
    const std::filesystem::path candidate_path = Utils::utf8_to_path(trimmed);
    if (!candidate_path.has_extension() && original_path.has_extension()) {
        return Utils::path_to_utf8(candidate_path) + Utils::path_to_utf8(original_path.extension());
    }
    return trimmed;
}

std::string display_category(const CategorizedFile& entry)
{
    return entry.category.empty() ? entry.canonical_category : entry.category;
}

std::string display_subcategory(const CategorizedFile& entry)
{
    return entry.subcategory.empty() ? entry.canonical_subcategory : entry.subcategory;
}

std::string source_directory_for_database(const CategorizedFile& entry,
                                          const HeadlessReviewApplyService::Options& options)
{
    return options.include_subdirectories ? entry.file_path : options.base_dir;
}

DatabaseManager::ResolvedCategory resolve_category_for_storage(DatabaseManager& db_manager,
                                                               const CategorizedFile& entry,
                                                               const std::string& category,
                                                               const std::string& subcategory,
                                                               CategoryLanguage language)
{
    if (!entry.canonical_category.empty()) {
        const std::string canonical_subcategory =
            entry.canonical_subcategory.empty() ? subcategory : entry.canonical_subcategory;
        return db_manager.resolve_category(entry.canonical_category, canonical_subcategory);
    }
    return db_manager.resolve_category_for_language(category, subcategory, language);
}

void append_skipped(HeadlessReviewApplyService::Result& result,
                    HeadlessReviewApplyService::EntryResult entry_result)
{
    entry_result.skipped = true;
    ++result.skipped_count;
    result.entries.push_back(std::move(entry_result));
}

} // namespace

HeadlessReviewApplyService::HeadlessReviewApplyService(DatabaseManager* db_manager,
                                                       IStorageProvider& storage_provider,
                                                       std::shared_ptr<spdlog::logger> logger,
                                                       ReviewHistoryStore* history_store)
    : db_manager_(db_manager),
      storage_provider_(storage_provider),
      logger_(std::move(logger)),
      history_store_(history_store)
{
}

HeadlessReviewApplyService::Result
HeadlessReviewApplyService::apply(const std::vector<CategorizedFile>& entries,
                                  const Options& options) const
{
    Result result;
    result.planned_count = entries.size();
    result.entries.reserve(entries.size());

    std::vector<MoveRecord> move_history;
    move_history.reserve(entries.size());
    for (const auto& entry : entries) {
        apply_entry(entry, options, result, move_history);
    }

    result.undo_plan_saved = persist_undo_plan(options, move_history);
    return result;
}

void HeadlessReviewApplyService::apply_entry(const CategorizedFile& entry,
                                             const Options& options,
                                             Result& result,
                                             std::vector<MoveRecord>& move_history) const
{
    const std::string destination_name =
        resolve_destination_name(entry, options.apply_suggested_names && !entry.rename_applied);
    const bool rename_active = destination_name != entry.file_name;
    const auto source_path =
        Utils::utf8_to_path(entry.file_path) / Utils::utf8_to_path(entry.file_name);

    EntryResult entry_result;
    entry_result.source = Utils::path_to_utf8(source_path);
    entry_result.file_name = entry.file_name;
    entry_result.destination_name = destination_name;
    entry_result.category = display_category(entry);
    entry_result.subcategory = display_subcategory(entry);
    entry_result.rename_only = entry.rename_only;

    if (entry.rename_only && !options.apply_suggested_names) {
        entry_result.message = "Rename-only entry skipped by categorization operation.";
        append_skipped(result, std::move(entry_result));
        return;
    }

    if (rename_active) {
        std::string rename_error;
        if (!validate_filename(destination_name, &rename_error)) {
            entry_result.message = rename_error;
            append_skipped(result, std::move(entry_result));
            return;
        }
    }

    if (entry.rename_only || !options.move_categorized_entries) {
        const auto destination_path =
            Utils::utf8_to_path(entry.file_path) / Utils::utf8_to_path(destination_name);
        entry_result.destination = Utils::path_to_utf8(destination_path);
        if (!rename_active) {
            entry_result.message = entry.suggested_name.empty()
                                       ? "No rename suggested."
                                       : "No rename needed.";
            append_skipped(result, std::move(entry_result));
            return;
        }
        if (!options.apply_changes) {
            entry_result.message = "Waiting for review approval.";
            result.entries.push_back(std::move(entry_result));
            return;
        }
        const auto move_result = storage_provider_.move_entry(entry_result.source,
                                                              entry_result.destination);
        if (!move_result.success) {
            entry_result.message = move_result.message.empty() ? "Rename failed." : move_result.message;
            append_skipped(result, std::move(entry_result));
            return;
        }

        entry_result.renamed = true;
        ++result.renamed_count;
        move_history.push_back(MoveRecord{entry_result.source,
                                          entry_result.destination,
                                          move_result.metadata.size_bytes,
                                          move_result.metadata.mtime,
                                          move_result.metadata.stable_identity,
                                          move_result.metadata.revision_token});
        record_history_entry(entry,
                             ReviewHistoryStore::Operation::Rename,
                             entry_result.source,
                             entry_result.destination,
                             destination_name,
                             std::string(),
                             std::string(),
                             move_result.metadata);
        if (db_manager_) {
            DatabaseManager::ResolvedCategory resolved{0, "", ""};
            const std::string category = display_category(entry);
            if (!is_missing_category_label(category)) {
                const std::string subcategory = display_subcategory(entry);
                const std::string effective_subcategory =
                    is_missing_category_label(subcategory) ? category : subcategory;
                resolved = resolve_category_for_storage(*db_manager_,
                                                        entry,
                                                        category,
                                                        effective_subcategory,
                                                        options.category_language);
            }
            const std::string type_label = entry.type == FileType::Directory ? "D" : "F";
            db_manager_->remove_file_categorization(entry.file_path, entry.file_name, entry.type);
            db_manager_->insert_or_update_file_with_categorization(destination_name,
                                                                   type_label,
                                                                   entry.file_path,
                                                                   resolved,
                                                                   entry.used_consistency_hints,
                                                                   destination_name,
                                                                   entry.rename_only || resolved.category.empty(),
                                                                   true);
        }
        result.entries.push_back(std::move(entry_result));
        return;
    }

    const std::string category = display_category(entry);
    const std::string subcategory = display_subcategory(entry);
    if (is_missing_category_label(category)) {
        entry_result.message = "Category is missing.";
        append_skipped(result, std::move(entry_result));
        return;
    }

    const std::string effective_subcategory =
        is_missing_category_label(subcategory) ? category : subcategory;
    std::string validation_error;
    if (!validate_label_pair(category,
                             effective_subcategory,
                             !options.use_subcategories,
                             &validation_error)) {
        entry_result.message = validation_error;
        append_skipped(result, std::move(entry_result));
        return;
    }

    try {
        MovableCategorizedFile movable(storage_provider_,
                                       entry.file_path,
                                       options.base_dir,
                                       category,
                                       effective_subcategory,
                                       entry.file_name,
                                       destination_name);
        const auto preview = movable.preview_move_paths(options.use_subcategories);
        entry_result.destination = preview.destination;

        if (!options.apply_changes) {
            entry_result.message = "Waiting for review approval.";
            result.entries.push_back(std::move(entry_result));
            return;
        }

        movable.create_cat_dirs(options.use_subcategories);
        const auto move_result = movable.move_file(options.use_subcategories);
        if (!move_result.success) {
            entry_result.message = move_result.message.empty() ? "Move failed." : move_result.message;
            append_skipped(result, std::move(entry_result));
            return;
        }

        entry_result.moved = true;
        entry_result.renamed = rename_active;
        ++result.moved_count;
        if (rename_active) {
            ++result.renamed_count;
        }
        move_history.push_back(MoveRecord{preview.source,
                                          preview.destination,
                                          move_result.metadata.size_bytes,
                                          move_result.metadata.mtime,
                                          move_result.metadata.stable_identity,
                                          move_result.metadata.revision_token});
        record_history_entry(entry,
                             rename_active ? ReviewHistoryStore::Operation::RenameAndCategorize
                                           : ReviewHistoryStore::Operation::Categorize,
                             preview.source,
                             preview.destination,
                             destination_name,
                             category,
                             effective_subcategory,
                             move_result.metadata);

        if (db_manager_ && (rename_active || options.include_subdirectories)) {
            auto resolved = resolve_category_for_storage(*db_manager_,
                                                         entry,
                                                         category,
                                                         effective_subcategory,
                                                         options.category_language);
            const std::string source_db_dir = source_directory_for_database(entry, options);
            std::string destination_db_dir = options.base_dir;
            if (options.include_subdirectories) {
                destination_db_dir =
                    Utils::path_to_utf8(Utils::utf8_to_path(preview.destination).parent_path());
            }
            std::string suggested_name = rename_active ? destination_name : entry.suggested_name;
            bool rename_applied = rename_active ? true : entry.rename_applied;
            db_manager_->remove_file_categorization(source_db_dir, entry.file_name, entry.type);
            db_manager_->insert_or_update_file_with_categorization(destination_name,
                                                                   entry.type == FileType::Directory ? "D" : "F",
                                                                   destination_db_dir,
                                                                   resolved,
                                                                   entry.used_consistency_hints,
                                                                   suggested_name,
                                                                   false,
                                                                   rename_applied);
        }
        result.entries.push_back(std::move(entry_result));
    } catch (const std::exception& ex) {
        entry_result.message = ex.what();
        append_skipped(result, std::move(entry_result));
        if (logger_) {
            logger_->warn("Headless apply skipped '{}': {}", entry.file_name, ex.what());
        }
    }
}

bool HeadlessReviewApplyService::persist_undo_plan(const Options& options,
                                                   const std::vector<MoveRecord>& move_history) const
{
    if (options.undo_dir.empty() || options.base_dir.empty() || move_history.empty()) {
        return false;
    }

    std::vector<UndoManager::Entry> entries;
    entries.reserve(move_history.size());
    for (const auto& record : move_history) {
        entries.push_back(UndoManager::Entry{
            record.source,
            record.destination,
            record.size_bytes,
            record.mtime,
            record.stable_identity,
            record.revision_token});
    }

    UndoManager manager(options.undo_dir);
    return manager.save_plan(options.base_dir,
                             storage_provider_.id(),
                             entries,
                             logger_);
}

void HeadlessReviewApplyService::record_history_entry(const CategorizedFile& entry,
                                                      ReviewHistoryStore::Operation operation,
                                                      const std::string& source,
                                                      const std::string& destination,
                                                      const std::string& destination_name,
                                                      const std::string& category,
                                                      const std::string& subcategory,
                                                      const StorageEntryMetadata& metadata) const
{
    if (!history_store_ || !history_store_->is_open()) {
        return;
    }

    ReviewHistoryStore::Entry history_entry;
    history_entry.provider_id = storage_provider_.id();
    history_entry.operation = operation;
    history_entry.source_path = source;
    history_entry.destination_path = destination;
    history_entry.original_file_name = entry.file_name;
    history_entry.final_file_name = destination_name;
    history_entry.category = category;
    history_entry.subcategory = subcategory;
    history_entry.file_description = strip_history_description_label(entry.learning_context);
    history_entry.size_bytes = metadata.size_bytes;
    history_entry.mtime = metadata.mtime;
    history_entry.stable_identity = metadata.stable_identity;
    history_entry.revision_token = metadata.revision_token;

    std::string error;
    if (!history_store_->record_entry(history_entry, &error) && logger_) {
        logger_->warn("Failed to record review history for '{}': {}",
                      entry.file_name,
                      error);
    }
}
