#include "ContentAnalysisCache.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "SqliteSavepoint.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <memory>
#include <sqlite3.h>

namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

Statement prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* statement = nullptr;
    if (!db || sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Content analysis cache statement failed: {}", db ? sqlite3_errmsg(db) : "database unavailable");
        }
        sqlite3_finalize(statement);
        statement = nullptr;
    }
    return Statement(statement, sqlite3_finalize);
}

void bind_key(sqlite3_stmt* statement, const ContentAnalysisCache::Key& key)
{
    sqlite3_bind_text(statement, 1, key.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, key.analyzer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, key.sha256.c_str(), -1, SQLITE_TRANSIENT);
}

bool complete(const ContentAnalysisCache::Evidence& evidence)
{
    return !QString::fromStdString(evidence.suggested_name).trimmed().isEmpty() &&
           !QString::fromStdString(evidence.description).trimmed().isEmpty();
}
} // namespace

bool ContentAnalysisCache::initialize() const
{
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS content_analysis_cache (
            source_path TEXT NOT NULL,
            dir_path TEXT NOT NULL,
            analyzer TEXT NOT NULL,
            sha256 TEXT NOT NULL,
            suggested_name TEXT NOT NULL,
            description TEXT NOT NULL,
            PRIMARY KEY(source_path, analyzer)
        );
    )";
    auto statement = prepare(db_, sql);
    return statement && sqlite3_step(statement.get()) == SQLITE_DONE &&
        sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS idx_content_analysis_cache_directory "
                         "ON content_analysis_cache(dir_path);", nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::optional<ContentAnalysisCache::Key> ContentAnalysisCache::make_key(
    const std::filesystem::path& path, const std::string& analyzer)
{
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error).lexically_normal();
    if (error || analyzer.empty()) {
        return std::nullopt;
    }
    QFile file(QString::fromStdString(Utils::path_to_utf8(absolute)));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return std::nullopt;
    }
    return Key{Utils::path_to_utf8(absolute), analyzer, hash.result().toHex().toStdString()};
}

std::optional<ContentAnalysisCache::Evidence> ContentAnalysisCache::find(const Key& key) const
{
    if (ensure_paths_ && !ensure_paths_()) return std::nullopt;
    auto statement = prepare(db_, "SELECT suggested_name, description FROM content_analysis_cache "
                                  "WHERE source_path IN (SELECT path FROM temp.aifs_path_alias WHERE comparison_key = aifs_path_key(?)) AND analyzer = ? AND sha256 = ? "
                                  "ORDER BY rowid DESC LIMIT 1;");
    if (!statement) {
        return std::nullopt;
    }
    bind_key(statement.get(), key);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    const auto text = [&](int column) {
        const auto* value = sqlite3_column_text(statement.get(), column);
        return value ? std::string(reinterpret_cast<const char*>(value)) : std::string();
    };
    Evidence evidence{text(0), text(1)};
    return complete(evidence) ? std::optional(std::move(evidence)) : std::nullopt;
}

bool ContentAnalysisCache::store(const Key& key, const Evidence& evidence) const
{
    if (!complete(evidence)) {
        return false;
    }
    const auto current = make_key(Utils::utf8_to_path(key.path), key.analyzer);
    if (!current || current->sha256 != key.sha256) {
        return false;
    }
    if (ensure_paths_ && !ensure_paths_()) return false;
    SqliteSavepoint write(db_);
    if (!write) return false;
    auto statement = prepare(db_, R"(
        INSERT INTO content_analysis_cache(source_path, analyzer, sha256, suggested_name, description, dir_path)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(source_path, analyzer) DO UPDATE SET
            sha256 = excluded.sha256,
            suggested_name = excluded.suggested_name,
            description = excluded.description;
    )");
    if (!statement) {
        return false;
    }
    bind_key(statement.get(), key);
    sqlite3_bind_text(statement.get(), 4, evidence.suggested_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 5, evidence.description.c_str(), -1, SQLITE_TRANSIENT);
    const auto directory = Utils::path_to_utf8(Utils::utf8_to_path(key.path).parent_path());
    sqlite3_bind_text(statement.get(), 6, directory.c_str(), -1, SQLITE_TRANSIENT);
    const bool stored = sqlite3_step(statement.get()) == SQLITE_DONE;
    if (!stored) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Could not store content analysis evidence: {}", sqlite3_errmsg(db_));
        }
    }
    if (!stored) return false;
    auto cleanup = prepare(db_, "DELETE FROM content_analysis_cache "
        "WHERE source_path IN (SELECT path FROM temp.aifs_path_alias WHERE comparison_key = aifs_path_key(?1)) AND analyzer = ?2 AND source_path != ?1;");
    if (!cleanup) return false;
    sqlite3_bind_text(cleanup.get(), 1, key.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(cleanup.get(), 2, key.analyzer.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(cleanup.get()) == SQLITE_DONE && write.commit();
}

bool ContentAnalysisCache::clear_directory(const std::string& directory, bool recursive) const
{
    if (ensure_paths_ && !ensure_paths_()) return false;
    auto statement = prepare(db_, recursive
        ? "DELETE FROM content_analysis_cache WHERE aifs_path_within(dir_path, ?1, 1);"
        : "DELETE FROM content_analysis_cache WHERE dir_path IN "
          "(SELECT path FROM temp.aifs_path_alias WHERE comparison_key = aifs_path_key(?1));");
    if (!statement) {
        return false;
    }
    std::error_code error;
    const auto absolute = std::filesystem::absolute(Utils::utf8_to_path(directory), error);
    if (error) {
        return false;
    }
    const auto normalized = Utils::path_to_utf8(absolute.lexically_normal());
    sqlite3_bind_text(statement.get(), 1, normalized.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(statement.get()) == SQLITE_DONE;
}
