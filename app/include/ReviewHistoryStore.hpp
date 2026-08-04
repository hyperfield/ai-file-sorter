#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include <sqlite3.h>

/**
 * @brief Persistent store for user-applied rename and categorization review history.
 */
class ReviewHistoryStore {
public:
    /**
     * @brief Type of operation recorded for an applied review entry.
     */
    enum class Operation {
        Rename,
        Categorize,
        RenameAndCategorize
    };

    /**
     * @brief One applied rename/categorization history row.
     */
    struct Entry {
        long long id{0};
        std::string created_at_utc;
        std::string provider_id;
        Operation operation{Operation::Categorize};
        std::string source_path;
        std::string destination_path;
        std::string original_file_name;
        std::string final_file_name;
        std::string category;
        std::string subcategory;
        std::string file_description;
        std::uintmax_t size_bytes{0};
        std::time_t mtime{0};
        std::string stable_identity;
        std::string revision_token;
        bool undone{false};
        std::string undone_at_utc;
    };

    /**
     * @brief Opens or creates the review history database below the config directory.
     * @param config_dir Directory where app-owned persistent data is stored.
     */
    explicit ReviewHistoryStore(std::string config_dir);
    /**
     * @brief Closes the database connection.
     */
    ~ReviewHistoryStore();

    ReviewHistoryStore(const ReviewHistoryStore&) = delete;
    ReviewHistoryStore& operator=(const ReviewHistoryStore&) = delete;

    /**
     * @brief Returns whether the backing database opened and initialized successfully.
     * @return True when the store can be used.
     */
    bool is_open() const;
    /**
     * @brief Records a successfully applied review operation.
     * @param entry Entry to persist. The id and timestamps are assigned by the store.
     * @param error Optional output for failure details.
     * @return The new history row id, or std::nullopt on failure.
     */
    std::optional<long long> record_entry(const Entry& entry, std::string* error = nullptr);
    /**
     * @brief Returns history entries, newest first.
     * @param limit Maximum rows to return. Zero means no limit.
     * @return Matching history entries.
     */
    std::vector<Entry> entries(std::size_t limit = 0) const;
    /**
     * @brief Searches history entries by filename, category, description, or path.
     * @param query User search text.
     * @param limit Maximum rows to return. Zero means no limit.
     * @return Matching history entries, newest first.
     */
    std::vector<Entry> search_entries(const std::string& query, std::size_t limit = 0) const;
    /**
     * @brief Looks up one history entry by id.
     * @param id History row id.
     * @return Entry when found.
     */
    std::optional<Entry> entry_by_id(long long id) const;
    /**
     * @brief Marks a history entry as undone.
     * @param id History row id.
     * @param error Optional output for failure details.
     * @return True when the row was updated.
     */
    bool mark_undone(long long id, std::string* error = nullptr);

    /**
     * @brief Converts an operation to its stable storage key.
     * @param operation Operation value.
     * @return Stable string stored in the database.
     */
    static std::string operation_to_string(Operation operation);
    /**
     * @brief Converts a stable storage key to an operation value.
     * @param value Stored operation key.
     * @return Parsed operation value.
     */
    static Operation operation_from_string(const std::string& value);

private:
    /**
     * @brief Creates or migrates the SQLite schema used by the store.
     * @param error Optional output for failure details.
     * @return True when schema initialization succeeds.
     */
    bool initialize_schema(std::string* error);

    sqlite3* db_{nullptr};
    std::string config_dir_;
    std::string db_file_;
};
