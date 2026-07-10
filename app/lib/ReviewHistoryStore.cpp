#include "ReviewHistoryStore.hpp"

#include "Logger.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>

namespace {

struct StatementDeleter {
    void operator()(sqlite3_stmt* stmt) const
    {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

std::string utc_now_iso()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now_time);
#else
    gmtime_r(&now_time, &utc);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

std::string sqlite_text(sqlite3_stmt* stmt, int column)
{
    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return value ? std::string(value) : std::string();
}

sqlite3_int64 clamp_sqlite_int64(std::uintmax_t value)
{
    constexpr auto max_value = static_cast<std::uintmax_t>(std::numeric_limits<sqlite3_int64>::max());
    return static_cast<sqlite3_int64>(std::min(value, max_value));
}

bool exec_sql(sqlite3* db, const char* sql, std::string* error)
{
    char* raw_error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &raw_error) == SQLITE_OK) {
        return true;
    }

    if (error) {
        *error = raw_error ? raw_error : sqlite3_errmsg(db);
    }
    if (auto logger = Logger::get_logger("db_logger")) {
        logger->error("Review history database error: {}",
                      raw_error ? raw_error : sqlite3_errmsg(db));
    }
    if (raw_error) {
        sqlite3_free(raw_error);
    }
    return false;
}

StatementPtr prepare_statement(sqlite3* db, const char* sql, std::string* error = nullptr)
{
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) == SQLITE_OK) {
        return StatementPtr(raw);
    }
    if (error) {
        *error = sqlite3_errmsg(db);
    }
    if (auto logger = Logger::get_logger("db_logger")) {
        logger->error("Failed to prepare review history statement: {}", sqlite3_errmsg(db));
    }
    return nullptr;
}

void bind_text(sqlite3_stmt* stmt, int index, const std::string& value)
{
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

ReviewHistoryStore::Entry read_entry(sqlite3_stmt* stmt)
{
    ReviewHistoryStore::Entry entry;
    entry.id = sqlite3_column_int64(stmt, 0);
    entry.created_at_utc = sqlite_text(stmt, 1);
    entry.provider_id = sqlite_text(stmt, 2);
    entry.operation = ReviewHistoryStore::operation_from_string(sqlite_text(stmt, 3));
    entry.source_path = sqlite_text(stmt, 4);
    entry.destination_path = sqlite_text(stmt, 5);
    entry.original_file_name = sqlite_text(stmt, 6);
    entry.final_file_name = sqlite_text(stmt, 7);
    entry.category = sqlite_text(stmt, 8);
    entry.subcategory = sqlite_text(stmt, 9);
    entry.file_description = sqlite_text(stmt, 10);
    entry.size_bytes = static_cast<std::uintmax_t>(sqlite3_column_int64(stmt, 11));
    entry.mtime = static_cast<std::time_t>(sqlite3_column_int64(stmt, 12));
    entry.stable_identity = sqlite_text(stmt, 13);
    entry.revision_token = sqlite_text(stmt, 14);
    entry.undone = sqlite3_column_int(stmt, 15) != 0;
    entry.undone_at_utc = sqlite_text(stmt, 16);
    return entry;
}

std::vector<ReviewHistoryStore::Entry> collect_entries(sqlite3* db,
                                                       const char* sql,
                                                       std::size_t limit,
                                                       const std::string* query = nullptr)
{
    auto stmt = prepare_statement(db, sql);
    if (!stmt) {
        return {};
    }

    int bind_index = 1;
    if (query) {
        const std::string pattern = "%" + *query + "%";
        for (int i = 0; i < 7; ++i) {
            bind_text(stmt.get(), bind_index++, pattern);
        }
    }
    if (limit > 0) {
        sqlite3_bind_int64(stmt.get(), bind_index, static_cast<sqlite3_int64>(limit));
    }

    std::vector<ReviewHistoryStore::Entry> rows;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        rows.push_back(read_entry(stmt.get()));
    }
    return rows;
}

} // namespace

ReviewHistoryStore::ReviewHistoryStore(std::string config_dir)
    : config_dir_(std::move(config_dir))
{
    if (config_dir_.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(Utils::utf8_to_path(config_dir_), ec);
    if (ec) {
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->error("Failed to create review history directory '{}': {}",
                          config_dir_,
                          ec.message());
        }
        return;
    }

    db_file_ = Utils::path_to_utf8(Utils::utf8_to_path(config_dir_) / "review_history.sqlite");
    if (sqlite3_open(db_file_.c_str(), &db_) != SQLITE_OK) {
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->error("Failed to open review history database '{}': {}",
                          db_file_,
                          db_ ? sqlite3_errmsg(db_) : "unknown error");
        }
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return;
    }

    std::string error;
    if (!initialize_schema(&error)) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

ReviewHistoryStore::~ReviewHistoryStore()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool ReviewHistoryStore::is_open() const
{
    return db_ != nullptr;
}

std::optional<long long> ReviewHistoryStore::record_entry(const Entry& entry, std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "Review history database is not open.";
        }
        return std::nullopt;
    }

    static constexpr char kSql[] = R"sql(
        INSERT INTO review_history (
            created_at_utc,
            provider_id,
            operation,
            source_path,
            destination_path,
            original_file_name,
            final_file_name,
            category,
            subcategory,
            file_description,
            size_bytes,
            mtime,
            stable_identity,
            revision_token,
            undone,
            undone_at_utc
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, '')
    )sql";

    auto stmt = prepare_statement(db_, kSql, error);
    if (!stmt) {
        return std::nullopt;
    }

    int index = 1;
    bind_text(stmt.get(), index++, utc_now_iso());
    bind_text(stmt.get(), index++, entry.provider_id.empty() ? std::string("local_fs") : entry.provider_id);
    bind_text(stmt.get(), index++, operation_to_string(entry.operation));
    bind_text(stmt.get(), index++, entry.source_path);
    bind_text(stmt.get(), index++, entry.destination_path);
    bind_text(stmt.get(), index++, entry.original_file_name);
    bind_text(stmt.get(), index++, entry.final_file_name);
    bind_text(stmt.get(), index++, entry.category);
    bind_text(stmt.get(), index++, entry.subcategory);
    bind_text(stmt.get(), index++, entry.file_description);
    sqlite3_bind_int64(stmt.get(), index++, clamp_sqlite_int64(entry.size_bytes));
    sqlite3_bind_int64(stmt.get(), index++, static_cast<sqlite3_int64>(entry.mtime));
    bind_text(stmt.get(), index++, entry.stable_identity);
    bind_text(stmt.get(), index++, entry.revision_token);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        if (error) {
            *error = sqlite3_errmsg(db_);
        }
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->error("Failed to insert review history entry: {}", sqlite3_errmsg(db_));
        }
        return std::nullopt;
    }
    return sqlite3_last_insert_rowid(db_);
}

std::vector<ReviewHistoryStore::Entry> ReviewHistoryStore::entries(std::size_t limit) const
{
    if (!db_) {
        return {};
    }
    static constexpr char kSqlNoLimit[] = R"sql(
        SELECT id, created_at_utc, provider_id, operation, source_path, destination_path,
               original_file_name, final_file_name, category, subcategory, file_description,
               size_bytes, mtime, stable_identity, revision_token, undone, undone_at_utc
        FROM review_history
        ORDER BY id DESC
    )sql";
    static constexpr char kSqlLimit[] = R"sql(
        SELECT id, created_at_utc, provider_id, operation, source_path, destination_path,
               original_file_name, final_file_name, category, subcategory, file_description,
               size_bytes, mtime, stable_identity, revision_token, undone, undone_at_utc
        FROM review_history
        ORDER BY id DESC
        LIMIT ?
    )sql";
    return collect_entries(db_, limit > 0 ? kSqlLimit : kSqlNoLimit, limit);
}

std::vector<ReviewHistoryStore::Entry>
ReviewHistoryStore::search_entries(const std::string& query, std::size_t limit) const
{
    if (!db_) {
        return {};
    }
    const std::string trimmed = trim_copy(query);
    if (trimmed.empty()) {
        return entries(limit);
    }

    static constexpr char kSqlNoLimit[] = R"sql(
        SELECT id, created_at_utc, provider_id, operation, source_path, destination_path,
               original_file_name, final_file_name, category, subcategory, file_description,
               size_bytes, mtime, stable_identity, revision_token, undone, undone_at_utc
        FROM review_history
        WHERE lower(original_file_name) LIKE lower(?)
           OR lower(final_file_name) LIKE lower(?)
           OR lower(category) LIKE lower(?)
           OR lower(subcategory) LIKE lower(?)
           OR lower(file_description) LIKE lower(?)
           OR lower(source_path) LIKE lower(?)
           OR lower(destination_path) LIKE lower(?)
        ORDER BY id DESC
    )sql";
    static constexpr char kSqlLimit[] = R"sql(
        SELECT id, created_at_utc, provider_id, operation, source_path, destination_path,
               original_file_name, final_file_name, category, subcategory, file_description,
               size_bytes, mtime, stable_identity, revision_token, undone, undone_at_utc
        FROM review_history
        WHERE lower(original_file_name) LIKE lower(?)
           OR lower(final_file_name) LIKE lower(?)
           OR lower(category) LIKE lower(?)
           OR lower(subcategory) LIKE lower(?)
           OR lower(file_description) LIKE lower(?)
           OR lower(source_path) LIKE lower(?)
           OR lower(destination_path) LIKE lower(?)
        ORDER BY id DESC
        LIMIT ?
    )sql";
    return collect_entries(db_, limit > 0 ? kSqlLimit : kSqlNoLimit, limit, &trimmed);
}

std::optional<ReviewHistoryStore::Entry> ReviewHistoryStore::entry_by_id(long long id) const
{
    if (!db_) {
        return std::nullopt;
    }
    static constexpr char kSql[] = R"sql(
        SELECT id, created_at_utc, provider_id, operation, source_path, destination_path,
               original_file_name, final_file_name, category, subcategory, file_description,
               size_bytes, mtime, stable_identity, revision_token, undone, undone_at_utc
        FROM review_history
        WHERE id = ?
    )sql";
    auto stmt = prepare_statement(db_, kSql);
    if (!stmt) {
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return read_entry(stmt.get());
    }
    return std::nullopt;
}

bool ReviewHistoryStore::mark_undone(long long id, std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "Review history database is not open.";
        }
        return false;
    }
    static constexpr char kSql[] = R"sql(
        UPDATE review_history
        SET undone = 1,
            undone_at_utc = ?
        WHERE id = ?
    )sql";
    auto stmt = prepare_statement(db_, kSql, error);
    if (!stmt) {
        return false;
    }
    bind_text(stmt.get(), 1, utc_now_iso());
    sqlite3_bind_int64(stmt.get(), 2, id);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        if (error) {
            *error = sqlite3_errmsg(db_);
        }
        return false;
    }
    return sqlite3_changes(db_) > 0;
}

std::string ReviewHistoryStore::operation_to_string(Operation operation)
{
    switch (operation) {
    case Operation::Rename:
        return "rename";
    case Operation::RenameAndCategorize:
        return "rename_and_categorize";
    case Operation::Categorize:
    default:
        return "categorize";
    }
}

ReviewHistoryStore::Operation ReviewHistoryStore::operation_from_string(const std::string& value)
{
    if (value == "rename") {
        return Operation::Rename;
    }
    if (value == "rename_and_categorize" || value == "categorize_and_rename") {
        return Operation::RenameAndCategorize;
    }
    return Operation::Categorize;
}

bool ReviewHistoryStore::initialize_schema(std::string* error)
{
    static constexpr char kSchema[] = R"sql(
        PRAGMA journal_mode = WAL;
        CREATE TABLE IF NOT EXISTS review_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            created_at_utc TEXT NOT NULL,
            provider_id TEXT NOT NULL DEFAULT 'local_fs',
            operation TEXT NOT NULL,
            source_path TEXT NOT NULL,
            destination_path TEXT NOT NULL,
            original_file_name TEXT NOT NULL,
            final_file_name TEXT NOT NULL,
            category TEXT NOT NULL DEFAULT '',
            subcategory TEXT NOT NULL DEFAULT '',
            file_description TEXT NOT NULL DEFAULT '',
            size_bytes INTEGER NOT NULL DEFAULT 0,
            mtime INTEGER NOT NULL DEFAULT 0,
            stable_identity TEXT NOT NULL DEFAULT '',
            revision_token TEXT NOT NULL DEFAULT '',
            undone INTEGER NOT NULL DEFAULT 0,
            undone_at_utc TEXT NOT NULL DEFAULT ''
        );
        CREATE INDEX IF NOT EXISTS idx_review_history_created
            ON review_history(created_at_utc);
        CREATE INDEX IF NOT EXISTS idx_review_history_final_file
            ON review_history(final_file_name);
        CREATE INDEX IF NOT EXISTS idx_review_history_category
            ON review_history(category, subcategory);
    )sql";
    return exec_sql(db_, kSchema, error);
}
