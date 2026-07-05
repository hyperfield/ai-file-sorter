#pragma once

#include "CategoryLanguage.hpp"
#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

class DatabaseManager;
class IStorageProvider;
namespace spdlog { class logger; }

/**
 * @brief Builds and applies headless categorization review plans.
 */
class HeadlessReviewApplyService {
public:
    /**
     * @brief Options controlling how review entries are applied.
     */
    struct Options {
        /** @brief Folder selected by the caller and used as the destination root. */
        std::string base_dir;
        /** @brief Directory used to save undo plans for successful moves. */
        std::string undo_dir;
        /** @brief Whether subcategory folders should be created. */
        bool use_subcategories{true};
        /** @brief Whether entries may come from recursive child folders. */
        bool include_subdirectories{false};
        /** @brief Whether suggested filenames should be applied. */
        bool apply_suggested_names{false};
        /** @brief Whether categorized entries should be moved into category folders. */
        bool move_categorized_entries{true};
        /** @brief Language used by display labels when canonical labels are absent. */
        CategoryLanguage category_language{CategoryLanguage::English};
    };

    /**
     * @brief Per-entry review/apply outcome.
     */
    struct EntryResult {
        std::string source;
        std::string destination;
        std::string file_name;
        std::string destination_name;
        std::string category;
        std::string subcategory;
        std::string message;
        bool rename_only{false};
        bool moved{false};
        bool renamed{false};
        bool skipped{false};
    };

    /**
     * @brief Aggregate review/apply outcome.
     */
    struct Result {
        std::vector<EntryResult> entries;
        std::size_t planned_count{0};
        std::size_t moved_count{0};
        std::size_t renamed_count{0};
        std::size_t skipped_count{0};
        bool undo_plan_saved{false};
    };

    /**
     * @brief Construct a review/apply service.
     * @param db_manager Cache database to update after moves. May be null.
     * @param storage_provider Storage provider used for move operations.
     * @param logger Logger for diagnostics.
     */
    HeadlessReviewApplyService(DatabaseManager* db_manager,
                               IStorageProvider& storage_provider,
                               std::shared_ptr<spdlog::logger> logger);

    /**
     * @brief Build review records and apply their moves.
     * @param entries Categorized review entries.
     * @param options Apply options.
     * @return Review/apply result.
     */
    Result apply(const std::vector<CategorizedFile>& entries, const Options& options) const;

private:
    struct MoveRecord {
        std::string source;
        std::string destination;
        std::uintmax_t size_bytes{0};
        std::time_t mtime{0};
        std::string stable_identity;
        std::string revision_token;
    };

    /**
     * @brief Apply one categorized entry.
     * @param entry Entry to apply.
     * @param options Apply options.
     * @param result Aggregate result to update.
     * @param move_history Successful move records for undo.
     */
    void apply_entry(const CategorizedFile& entry,
                     const Options& options,
                     Result& result,
                     std::vector<MoveRecord>& move_history) const;

    /**
     * @brief Persist an undo plan for successful moves.
     * @param options Apply options.
     * @param move_history Successful move records.
     * @return True when an undo plan was saved.
     */
    bool persist_undo_plan(const Options& options,
                           const std::vector<MoveRecord>& move_history) const;

    DatabaseManager* db_manager_{nullptr};
    IStorageProvider& storage_provider_;
    std::shared_ptr<spdlog::logger> logger_;
};
