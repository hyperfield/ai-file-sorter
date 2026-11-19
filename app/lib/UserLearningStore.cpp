#include "Utils.hpp"
#include "TextEncoding.hpp"
#include "UserLearningStore.hpp"

#include "Logger.hpp"
#include "TextEmbeddingService.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace {

struct StatementDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

const char* file_type_to_db(FileType type)
{
    return type == FileType::Directory ? "D" : "F";
}

FileType file_type_from_db(const char* value)
{
    return value && std::string(value) == "D" ? FileType::Directory : FileType::File;
}

std::string sqlite_text(sqlite3_stmt* stmt, int column)
{
    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return value ? std::string(value) : std::string();
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
        logger->error("User learning database error: {}", raw_error ? raw_error : sqlite3_errmsg(db));
    }
    if (raw_error) {
        sqlite3_free(raw_error);
    }
    return false;
}

StatementPtr prepare_statement(sqlite3* db, const char* sql, std::string* error)
{
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) == SQLITE_OK) {
        return StatementPtr(raw);
    }

    if (error) {
        *error = sqlite3_errmsg(db);
    }
    if (auto logger = Logger::get_logger("db_logger")) {
        logger->error("Failed to prepare user learning statement: {}", sqlite3_errmsg(db));
    }
    return nullptr;
}

std::string file_extension(const std::string& file_name)
{
    const auto dot = file_name.find_last_of('.');
    if (dot == std::string::npos || dot == file_name.size() - 1) {
        return {};
    }
    std::string ext = file_name.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(TextEncoding::ascii_lower(ch));
    });
    return ext;
}

std::string normalize_retrieval_token(std::string token)
{
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
        return static_cast<char>(TextEncoding::ascii_lower(ch));
    });

    if (token.size() > 4 && token.ends_with("ies")) {
        token.erase(token.size() - 3);
        token.push_back('y');
    } else if (token.size() > 4 && token.ends_with("es")) {
        token.erase(token.size() - 2);
    } else if (token.size() > 3 && token.back() == 's') {
        token.pop_back();
    }
    return token;
}

std::vector<std::string> tokenize_for_retrieval(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char ch : text) {
        if (TextEncoding::ascii_alnum(ch) || ch >= 128) {
            current.push_back(static_cast<char>(ch));
            continue;
        }
        if (!current.empty()) {
            tokens.push_back(normalize_retrieval_token(std::move(current)));
            current.clear();
        }
    }
    if (!current.empty()) {
        tokens.push_back(normalize_retrieval_token(std::move(current)));
    }
    tokens.erase(std::remove_if(tokens.begin(),
                                tokens.end(),
                                [](const std::string& token) {
                                    return token.size() < 2;
                                }),
                 tokens.end());
    return tokens;
}

std::unordered_set<std::string> make_token_set(const std::string& text)
{
    const auto tokens = tokenize_for_retrieval(text);
    return std::unordered_set<std::string>(tokens.begin(), tokens.end());
}

int score_retrieval_text(const std::unordered_set<std::string>& query_tokens,
                         const std::string& text,
                         int weight)
{
    if (query_tokens.empty() || text.empty()) {
        return 0;
    }

    int score = 0;
    std::unordered_set<std::string> seen;
    for (const auto& token : tokenize_for_retrieval(text)) {
        if (query_tokens.find(token) == query_tokens.end()) {
            continue;
        }
        if (seen.insert(token).second) {
            score += weight;
        }
    }
    return score;
}

std::string serialize_embedding_vector(const std::vector<float>& vector)
{
    std::string blob(vector.size() * sizeof(float), '\0');
    if (!blob.empty()) {
        std::memcpy(blob.data(), vector.data(), blob.size());
    }
    return blob;
}

std::vector<float> deserialize_embedding_vector(const void* data, int byte_count, int dimension)
{
    if (!data || dimension <= 0 ||
        byte_count != static_cast<int>(static_cast<std::size_t>(dimension) * sizeof(float))) {
        return {};
    }

    std::vector<float> vector(static_cast<std::size_t>(dimension), 0.0f);
    std::memcpy(vector.data(), data, static_cast<std::size_t>(byte_count));
    return vector;
}

int embedding_score(double similarity)
{
    constexpr double kMinimumUsefulSimilarity = 0.05;
    if (similarity < kMinimumUsefulSimilarity) {
        return 0;
    }
    return static_cast<int>(std::round(similarity * 100.0));
}

} // namespace

std::filesystem::path UserLearningStore::database_path_for_config_dir(const std::string& config_dir)
{
    return Utils::utf8_to_path(config_dir) / "user_learning.db";
}

UserLearningStore::UserLearningStore(std::string config_dir)
    : db_file_(database_path_for_config_dir(config_dir))
{
    std::error_code ec;
    std::filesystem::create_directories(db_file_.parent_path(), ec);
    if (ec) {
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->error("Failed to create user learning database directory '{}': {}",
                          Utils::path_to_utf8(db_file_.parent_path()),
                          ec.message());
        }
        return;
    }

    if (sqlite3_open(Utils::path_to_utf8(db_file_).c_str(), &db_) != SQLITE_OK) {
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->error("Can't open user learning database '{}': {}",
                          Utils::path_to_utf8(db_file_),
                          db_ ? sqlite3_errmsg(db_) : "unknown error");
        }
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return;
    }

    sqlite3_extended_result_codes(db_, 1);
    std::string error;
    if (!initialize_schema(&error)) {
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->error("Failed to initialize user learning database '{}': {}",
                          Utils::path_to_utf8(db_file_),
                          error);
        }
    } else if (!rebuild_taxonomy_embeddings(&error)) {
        if (auto logger = Logger::get_logger("db_logger")) {
            logger->warn("Failed to refresh user learning embeddings '{}': {}",
                         Utils::path_to_utf8(db_file_),
                         error);
        }
    }
}

UserLearningStore::~UserLearningStore()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool UserLearningStore::initialize_schema(std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }

    const char* sql = R"(
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS learning_meta (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );

        INSERT OR REPLACE INTO learning_meta(key, value)
        VALUES('schema_version', '2');

        CREATE TABLE IF NOT EXISTS learned_taxonomy_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category TEXT NOT NULL,
            subcategory TEXT NOT NULL DEFAULT '',
            normalized_category TEXT NOT NULL,
            normalized_subcategory TEXT NOT NULL,
            source TEXT NOT NULL DEFAULT 'review_confirmed',
            example_count INTEGER NOT NULL DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(normalized_category, normalized_subcategory)
        );

        CREATE TABLE IF NOT EXISTS approved_category_examples (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            taxonomy_entry_id INTEGER NOT NULL,
            file_name TEXT NOT NULL,
            file_type TEXT NOT NULL,
            dir_path TEXT NOT NULL,
            extension TEXT NOT NULL DEFAULT '',
            suggested_name TEXT NOT NULL DEFAULT '',
            context_text TEXT NOT NULL DEFAULT '',
            source TEXT NOT NULL DEFAULT 'review_confirmed',
            used_consistency_hints INTEGER NOT NULL DEFAULT 0,
            approved_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(file_name, file_type, dir_path),
            FOREIGN KEY(taxonomy_entry_id) REFERENCES learned_taxonomy_entries(id)
        );

        CREATE INDEX IF NOT EXISTS idx_approved_examples_taxonomy
        ON approved_category_examples(taxonomy_entry_id);

        CREATE INDEX IF NOT EXISTS idx_approved_examples_extension
        ON approved_category_examples(extension);

        CREATE TABLE IF NOT EXISTS learned_category_aliases (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            taxonomy_entry_id INTEGER NOT NULL,
            alias_category TEXT NOT NULL,
            alias_subcategory TEXT NOT NULL DEFAULT '',
            normalized_alias_category TEXT NOT NULL,
            normalized_alias_subcategory TEXT NOT NULL,
            source TEXT NOT NULL DEFAULT 'user',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(normalized_alias_category, normalized_alias_subcategory),
            FOREIGN KEY(taxonomy_entry_id) REFERENCES learned_taxonomy_entries(id)
        );

        CREATE TABLE IF NOT EXISTS embedding_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS taxonomy_embeddings (
            taxonomy_entry_id INTEGER NOT NULL,
            embedding_model TEXT NOT NULL,
            source_text_hash TEXT NOT NULL,
            dimension INTEGER NOT NULL,
            vector BLOB NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(taxonomy_entry_id, embedding_model),
            FOREIGN KEY(taxonomy_entry_id) REFERENCES learned_taxonomy_entries(id)
        );
    )";

    if (!exec_sql(db_, sql, error)) {
        return false;
    }

    std::ostringstream metadata_sql;
    metadata_sql
        << "INSERT OR REPLACE INTO embedding_metadata(key, value, updated_at) VALUES"
        << "('active_embedding_model', '" << TextEmbeddingService::model_id() << "', CURRENT_TIMESTAMP),"
        << "('active_embedding_dimension', '" << TextEmbeddingService::dimension() << "', CURRENT_TIMESTAMP);";
    return exec_sql(db_, metadata_sql.str().c_str(), error);
}

bool UserLearningStore::record_approved_mapping(const ApprovedMapping& mapping, std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }
    if (mapping.category.empty()) {
        return true;
    }

    if (!exec_sql(db_, "BEGIN IMMEDIATE TRANSACTION;", error)) {
        return false;
    }

    const std::optional<int> old_taxonomy_id = existing_example_taxonomy_id(mapping);
    const int taxonomy_id = resolve_taxonomy_entry(mapping, error);
    bool success = taxonomy_id > 0 &&
                   upsert_approved_example(mapping, taxonomy_id, old_taxonomy_id, error);
    if (success) {
        success = exec_sql(db_, "COMMIT;", error);
    } else {
        std::string rollback_error;
        exec_sql(db_, "ROLLBACK;", &rollback_error);
    }
    return success;
}

int UserLearningStore::resolve_taxonomy_entry(const ApprovedMapping& mapping, std::string* error)
{
    return resolve_taxonomy_entry(mapping.category,
                                  mapping.subcategory,
                                  mapping.source,
                                  /*preserve_existing_source=*/false,
                                  error);
}

bool UserLearningStore::record_taxonomy_candidate(const TaxonomyCandidate& candidate, std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }
    if (normalize_label(candidate.category).empty()) {
        return true;
    }
    const int taxonomy_id = resolve_taxonomy_entry(candidate.category,
                                                   candidate.subcategory,
                                                   candidate.source,
                                                   /*preserve_existing_source=*/true,
                                                   error);
    return taxonomy_id > 0 && refresh_taxonomy_embedding(taxonomy_id, error);
}

bool UserLearningStore::import_taxonomy_candidates(const std::vector<TaxonomyCandidate>& candidates,
                                                   std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }

    if (!exec_sql(db_, "BEGIN IMMEDIATE TRANSACTION;", error)) {
        return false;
    }

    bool success = true;
    for (const auto& candidate : candidates) {
        if (normalize_label(candidate.category).empty()) {
            continue;
        }
        const int taxonomy_id = resolve_taxonomy_entry(candidate.category,
                                                       candidate.subcategory,
                                                       candidate.source,
                                                       /*preserve_existing_source=*/true,
                                                       error);
        if (taxonomy_id <= 0 || !refresh_taxonomy_embedding(taxonomy_id, error)) {
            success = false;
            break;
        }
    }

    if (success) {
        success = exec_sql(db_, "COMMIT;", error);
    } else {
        std::string rollback_error;
        exec_sql(db_, "ROLLBACK;", &rollback_error);
    }
    return success;
}

bool UserLearningStore::remove_taxonomy_candidates_with_source_prefix(const std::string& source_prefix,
                                                                      std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }
    if (source_prefix.empty()) {
        return true;
    }

    if (!exec_sql(db_, "BEGIN IMMEDIATE TRANSACTION;", error)) {
        return false;
    }

    const std::string source_pattern = source_prefix + "%";
    const auto execute_delete = [&](const char* sql) -> bool {
        auto stmt = prepare_statement(db_, sql, error);
        if (!stmt) {
            return false;
        }
        sqlite3_bind_text(stmt.get(), 1, source_pattern.c_str(), -1, SQLITE_TRANSIENT);
        return sqlite3_step(stmt.get()) == SQLITE_DONE;
    };

    bool success =
        execute_delete(
            "DELETE FROM taxonomy_embeddings "
            "WHERE taxonomy_entry_id IN ("
            "    SELECT id FROM learned_taxonomy_entries "
            "    WHERE source LIKE ? AND example_count = 0"
            ");") &&
        execute_delete(
            "DELETE FROM learned_category_aliases "
            "WHERE taxonomy_entry_id IN ("
            "    SELECT id FROM learned_taxonomy_entries "
            "    WHERE source LIKE ? AND example_count = 0"
            ");") &&
        execute_delete(
            "DELETE FROM approved_category_examples "
            "WHERE taxonomy_entry_id IN ("
            "    SELECT id FROM learned_taxonomy_entries "
            "    WHERE source LIKE ? AND example_count = 0"
            ");") &&
        execute_delete(
            "DELETE FROM learned_taxonomy_entries "
            "WHERE source LIKE ? AND example_count = 0;");

    if (success) {
        success = exec_sql(db_, "COMMIT;", error);
    } else {
        std::string rollback_error;
        exec_sql(db_, "ROLLBACK;", &rollback_error);
        if (error && error->empty()) {
            *error = "Failed to delete imported taxonomy candidates.";
        }
    }

    return success;
}

int UserLearningStore::resolve_taxonomy_entry(const std::string& category,
                                              const std::string& subcategory,
                                              const std::string& source,
                                              bool preserve_existing_source,
                                              std::string* error)
{
    const std::string normalized_category = normalize_label(category);
    const std::string normalized_subcategory = normalize_label(subcategory);
    if (normalized_category.empty()) {
        if (error) {
            *error = "Approved category is empty.";
        }
        return 0;
    }

    {
        const char* sql = R"(
            SELECT id, source FROM learned_taxonomy_entries
            WHERE normalized_category = ? AND normalized_subcategory = ?;
        )";
        auto stmt = prepare_statement(db_, sql, error);
        if (!stmt) {
            return 0;
        }
        sqlite3_bind_text(stmt.get(), 1, normalized_category.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, normalized_subcategory.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            const int id = sqlite3_column_int(stmt.get(), 0);
            const char* update_sql = preserve_existing_source
                                         ? R"(
                                               UPDATE learned_taxonomy_entries
                                               SET category = ?, subcategory = ?, updated_at = CURRENT_TIMESTAMP
                                               WHERE id = ?;
                                           )"
                                         : R"(
                                               UPDATE learned_taxonomy_entries
                                               SET category = ?, subcategory = ?, source = ?, updated_at = CURRENT_TIMESTAMP
                                               WHERE id = ?;
                                           )";
            auto update = prepare_statement(db_, update_sql, error);
            if (!update) {
                return 0;
            }
            sqlite3_bind_text(update.get(), 1, category.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(update.get(), 2, subcategory.c_str(), -1, SQLITE_TRANSIENT);
            if (preserve_existing_source) {
                sqlite3_bind_int(update.get(), 3, id);
            } else {
                sqlite3_bind_text(update.get(), 3, source.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(update.get(), 4, id);
            }
            if (sqlite3_step(update.get()) != SQLITE_DONE) {
                if (error) {
                    *error = sqlite3_errmsg(db_);
                }
                return 0;
            }
            return id;
        }
    }

    const char* insert_sql = R"(
        INSERT INTO learned_taxonomy_entries(
            category,
            subcategory,
            normalized_category,
            normalized_subcategory,
            source
        ) VALUES (?, ?, ?, ?, ?);
    )";
    auto insert = prepare_statement(db_, insert_sql, error);
    if (!insert) {
        return 0;
    }
    sqlite3_bind_text(insert.get(), 1, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 2, subcategory.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 3, normalized_category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 4, normalized_subcategory.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 5, source.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(insert.get()) != SQLITE_DONE) {
        if (error) {
            *error = sqlite3_errmsg(db_);
        }
        return 0;
    }
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

bool UserLearningStore::upsert_approved_example(const ApprovedMapping& mapping,
                                                int taxonomy_entry_id,
                                                std::optional<int> old_taxonomy_entry_id,
                                                std::string* error)
{
    const char* sql = R"(
        INSERT INTO approved_category_examples(
            taxonomy_entry_id,
            file_name,
            file_type,
            dir_path,
            extension,
            suggested_name,
            context_text,
            source,
            used_consistency_hints
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(file_name, file_type, dir_path) DO UPDATE SET
            taxonomy_entry_id = excluded.taxonomy_entry_id,
            extension = excluded.extension,
            suggested_name = excluded.suggested_name,
            context_text = excluded.context_text,
            source = excluded.source,
            used_consistency_hints = excluded.used_consistency_hints,
            approved_at = CURRENT_TIMESTAMP;
    )";

    auto stmt = prepare_statement(db_, sql, error);
    if (!stmt) {
        return false;
    }

    const std::string type = file_type_to_db(mapping.file_type);
    const std::string ext = file_extension(mapping.file_name);
    sqlite3_bind_int(stmt.get(), 1, taxonomy_entry_id);
    sqlite3_bind_text(stmt.get(), 2, mapping.file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 4, mapping.dir_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 5, ext.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 6, mapping.suggested_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 7, mapping.context_text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 8, mapping.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 9, mapping.used_consistency_hints ? 1 : 0);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        if (error) {
            *error = sqlite3_errmsg(db_);
        }
        return false;
    }

    if (old_taxonomy_entry_id && *old_taxonomy_entry_id != taxonomy_entry_id) {
        if (!refresh_example_count(*old_taxonomy_entry_id, error)) {
            return false;
        }
        if (!refresh_taxonomy_embedding(*old_taxonomy_entry_id, error)) {
            return false;
        }
    }
    return refresh_example_count(taxonomy_entry_id, error) &&
           refresh_taxonomy_embedding(taxonomy_entry_id, error);
}

std::optional<int> UserLearningStore::existing_example_taxonomy_id(const ApprovedMapping& mapping) const
{
    if (!db_) {
        return std::nullopt;
    }
    const char* sql = R"(
        SELECT taxonomy_entry_id FROM approved_category_examples
        WHERE file_name = ? AND file_type = ? AND dir_path = ?;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return std::nullopt;
    }

    const std::string type = file_type_to_db(mapping.file_type);
    sqlite3_bind_text(stmt.get(), 1, mapping.file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, mapping.dir_path.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0);
    }
    return std::nullopt;
}

bool UserLearningStore::refresh_example_count(int taxonomy_entry_id, std::string* error)
{
    const char* sql = R"(
        UPDATE learned_taxonomy_entries
        SET example_count = (
                SELECT COUNT(*)
                FROM approved_category_examples
                WHERE taxonomy_entry_id = ?
            ),
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?;
    )";
    auto stmt = prepare_statement(db_, sql, error);
    if (!stmt) {
        return false;
    }
    sqlite3_bind_int(stmt.get(), 1, taxonomy_entry_id);
    sqlite3_bind_int(stmt.get(), 2, taxonomy_entry_id);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        if (error) {
            *error = sqlite3_errmsg(db_);
        }
        return false;
    }
    return true;
}

std::optional<UserLearningStore::TaxonomyEntry>
UserLearningStore::find_taxonomy_entry(const std::string& category,
                                       const std::string& subcategory) const
{
    if (!db_) {
        return std::nullopt;
    }

    const std::string normalized_category = normalize_label(category);
    const std::string normalized_subcategory = normalize_label(subcategory);
    const char* sql = R"(
        SELECT id, category, subcategory, example_count, source
        FROM learned_taxonomy_entries
        WHERE normalized_category = ? AND normalized_subcategory = ?;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt.get(), 1, normalized_category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, normalized_subcategory.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    return TaxonomyEntry{
        sqlite3_column_int(stmt.get(), 0),
        sqlite_text(stmt.get(), 1),
        sqlite_text(stmt.get(), 2),
        sqlite3_column_int(stmt.get(), 3),
        sqlite_text(stmt.get(), 4)
    };
}

std::vector<UserLearningStore::RetrievedCandidate>
UserLearningStore::retrieve_taxonomy_candidates(const std::string& query_text, std::size_t limit) const
{
    std::vector<RetrievedCandidate> candidates;
    if (!db_ || limit == 0) {
        return candidates;
    }

    const auto query_tokens = make_token_set(query_text);
    if (query_tokens.empty()) {
        return candidates;
    }
    const auto query_embedding = TextEmbeddingService::embed(query_text);

    const char* sql = R"(
        SELECT t.id,
               t.category,
               t.subcategory,
               t.example_count,
               t.source,
               e.file_name,
               e.dir_path,
               e.extension,
               e.suggested_name,
               e.context_text,
               emb.dimension,
               emb.vector
        FROM learned_taxonomy_entries t
        LEFT JOIN approved_category_examples e ON e.taxonomy_entry_id = t.id
        LEFT JOIN taxonomy_embeddings emb
               ON emb.taxonomy_entry_id = t.id
              AND emb.embedding_model = ?
        ORDER BY t.id, e.id;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return candidates;
    }
    const std::string embedding_model(TextEmbeddingService::model_id());
    sqlite3_bind_text(stmt.get(), 1, embedding_model.c_str(), -1, SQLITE_TRANSIENT);

    std::unordered_map<int, std::size_t> index_by_id;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const int taxonomy_id = sqlite3_column_int(stmt.get(), 0);
        auto index_it = index_by_id.find(taxonomy_id);
        if (index_it == index_by_id.end()) {
            RetrievedCandidate candidate;
            candidate.taxonomy_entry_id = taxonomy_id;
            candidate.category = sqlite_text(stmt.get(), 1);
            candidate.subcategory = sqlite_text(stmt.get(), 2);
            candidate.example_count = sqlite3_column_int(stmt.get(), 3);
            candidate.source = sqlite_text(stmt.get(), 4);
            candidate.score += score_retrieval_text(query_tokens, candidate.category, 6);
            candidate.score += score_retrieval_text(query_tokens, candidate.subcategory, 5);
            const auto stored_vector = deserialize_embedding_vector(
                sqlite3_column_blob(stmt.get(), 11),
                sqlite3_column_bytes(stmt.get(), 11),
                sqlite3_column_int(stmt.get(), 10));
            const double similarity =
                TextEmbeddingService::cosine_similarity(query_embedding, stored_vector);
            const int vector_score = embedding_score(similarity);
            if (vector_score > 0) {
                candidate.embedding_similarity = similarity;
                candidate.used_embedding = true;
                candidate.score += vector_score;
            }
            index_it = index_by_id.emplace(taxonomy_id, candidates.size()).first;
            candidates.push_back(std::move(candidate));
        }

        auto& candidate = candidates[index_it->second];
        candidate.score += score_retrieval_text(query_tokens, sqlite_text(stmt.get(), 5), 4);
        candidate.score += score_retrieval_text(query_tokens, sqlite_text(stmt.get(), 6), 2);
        candidate.score += score_retrieval_text(query_tokens, sqlite_text(stmt.get(), 7), 2);
        candidate.score += score_retrieval_text(query_tokens, sqlite_text(stmt.get(), 8), 3);
        candidate.score += score_retrieval_text(query_tokens, sqlite_text(stmt.get(), 9), 3);
    }

    candidates.erase(std::remove_if(candidates.begin(),
                                    candidates.end(),
                                    [](const RetrievedCandidate& candidate) {
                                        return candidate.score <= 0 || candidate.category.empty();
                                    }),
                     candidates.end());
    for (auto& candidate : candidates) {
        if (candidate.source == "review_confirmed") {
            candidate.score += 2;
        }
        candidate.score += std::min(candidate.example_count, 3);
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        if (lhs.used_embedding != rhs.used_embedding) {
            return lhs.used_embedding;
        }
        if (lhs.embedding_similarity != rhs.embedding_similarity) {
            return lhs.embedding_similarity > rhs.embedding_similarity;
        }
        if (lhs.example_count != rhs.example_count) {
            return lhs.example_count > rhs.example_count;
        }
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        return lhs.subcategory < rhs.subcategory;
    });
    if (candidates.size() > limit) {
        candidates.resize(limit);
    }
    return candidates;
}

bool UserLearningStore::rebuild_taxonomy_embeddings(std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }

    const char* sql = "SELECT id FROM learned_taxonomy_entries ORDER BY id;";
    auto stmt = prepare_statement(db_, sql, error);
    if (!stmt) {
        return false;
    }

    std::vector<int> taxonomy_ids;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        taxonomy_ids.push_back(sqlite3_column_int(stmt.get(), 0));
    }

    if (!exec_sql(db_, "BEGIN IMMEDIATE TRANSACTION;", error)) {
        return false;
    }

    bool success = true;
    for (int taxonomy_id : taxonomy_ids) {
        if (!refresh_taxonomy_embedding(taxonomy_id, error)) {
            success = false;
            break;
        }
    }

    if (success) {
        success = exec_sql(db_, "COMMIT;", error);
    } else {
        std::string rollback_error;
        exec_sql(db_, "ROLLBACK;", &rollback_error);
    }
    return success;
}

bool UserLearningStore::clear_all(std::string* error)
{
    if (!db_) {
        if (error) {
            *error = "User learning database is not open.";
        }
        return false;
    }

    const char* sql = R"(
        BEGIN IMMEDIATE TRANSACTION;
        DELETE FROM taxonomy_embeddings;
        DELETE FROM learned_category_aliases;
        DELETE FROM approved_category_examples;
        DELETE FROM learned_taxonomy_entries;
        COMMIT;
    )";
    if (exec_sql(db_, sql, error)) {
        return true;
    }

    std::string rollback_error;
    exec_sql(db_, "ROLLBACK;", &rollback_error);
    return false;
}

std::optional<UserLearningStore::TaxonomyEmbedding>
UserLearningStore::taxonomy_embedding(int taxonomy_entry_id, const std::string& embedding_model) const
{
    if (!db_ || taxonomy_entry_id <= 0) {
        return std::nullopt;
    }

    const std::string model = embedding_model.empty()
        ? std::string(TextEmbeddingService::model_id())
        : embedding_model;
    const char* sql = R"(
        SELECT taxonomy_entry_id, embedding_model, source_text_hash, dimension, vector
        FROM taxonomy_embeddings
        WHERE taxonomy_entry_id = ? AND embedding_model = ?;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return std::nullopt;
    }
    sqlite3_bind_int(stmt.get(), 1, taxonomy_entry_id);
    sqlite3_bind_text(stmt.get(), 2, model.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    TaxonomyEmbedding embedding;
    embedding.taxonomy_entry_id = sqlite3_column_int(stmt.get(), 0);
    embedding.embedding_model = sqlite_text(stmt.get(), 1);
    embedding.source_text_hash = sqlite_text(stmt.get(), 2);
    embedding.dimension = sqlite3_column_int(stmt.get(), 3);
    embedding.vector = deserialize_embedding_vector(sqlite3_column_blob(stmt.get(), 4),
                                                    sqlite3_column_bytes(stmt.get(), 4),
                                                    embedding.dimension);
    return embedding;
}

int UserLearningStore::taxonomy_embedding_count() const
{
    if (!db_) {
        return 0;
    }

    const char* sql = R"(
        SELECT COUNT(*)
        FROM taxonomy_embeddings
        WHERE embedding_model = ?;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return 0;
    }
    const std::string model(TextEmbeddingService::model_id());
    sqlite3_bind_text(stmt.get(), 1, model.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return 0;
    }
    return sqlite3_column_int(stmt.get(), 0);
}

bool UserLearningStore::refresh_taxonomy_embedding(int taxonomy_entry_id, std::string* error)
{
    if (!db_ || taxonomy_entry_id <= 0) {
        return true;
    }

    const std::string source_text = embedding_source_text_for_taxonomy_entry(taxonomy_entry_id);
    if (source_text.empty()) {
        const char* delete_sql = R"(
            DELETE FROM taxonomy_embeddings
            WHERE taxonomy_entry_id = ? AND embedding_model = ?;
        )";
        auto delete_stmt = prepare_statement(db_, delete_sql, error);
        if (!delete_stmt) {
            return false;
        }
        const std::string model(TextEmbeddingService::model_id());
        sqlite3_bind_int(delete_stmt.get(), 1, taxonomy_entry_id);
        sqlite3_bind_text(delete_stmt.get(), 2, model.c_str(), -1, SQLITE_TRANSIENT);
        return sqlite3_step(delete_stmt.get()) == SQLITE_DONE;
    }

    const auto vector = TextEmbeddingService::embed(source_text);
    const std::string blob = serialize_embedding_vector(vector);
    const std::string model(TextEmbeddingService::model_id());
    const std::string source_hash = TextEmbeddingService::source_hash(source_text);
    const char* sql = R"(
        INSERT INTO taxonomy_embeddings(
            taxonomy_entry_id,
            embedding_model,
            source_text_hash,
            dimension,
            vector
        ) VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(taxonomy_entry_id, embedding_model) DO UPDATE SET
            source_text_hash = excluded.source_text_hash,
            dimension = excluded.dimension,
            vector = excluded.vector,
            created_at = CURRENT_TIMESTAMP;
    )";
    auto stmt = prepare_statement(db_, sql, error);
    if (!stmt) {
        return false;
    }
    sqlite3_bind_int(stmt.get(), 1, taxonomy_entry_id);
    sqlite3_bind_text(stmt.get(), 2, model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, source_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 4, static_cast<int>(vector.size()));
    sqlite3_bind_blob(stmt.get(), 5, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        if (error) {
            *error = sqlite3_errmsg(db_);
        }
        return false;
    }
    return true;
}

std::string UserLearningStore::embedding_source_text_for_taxonomy_entry(int taxonomy_entry_id) const
{
    if (!db_ || taxonomy_entry_id <= 0) {
        return {};
    }

    const char* sql = R"(
        SELECT t.category,
               t.subcategory,
               e.file_name,
               e.dir_path,
               e.extension,
               e.suggested_name,
               e.context_text
        FROM learned_taxonomy_entries t
        LEFT JOIN approved_category_examples e ON e.taxonomy_entry_id = t.id
        WHERE t.id = ?
        ORDER BY e.id;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return {};
    }
    sqlite3_bind_int(stmt.get(), 1, taxonomy_entry_id);

    std::ostringstream source;
    bool wrote_taxonomy = false;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        if (!wrote_taxonomy) {
            source << "Category: " << sqlite_text(stmt.get(), 0) << "\n";
            const std::string subcategory = sqlite_text(stmt.get(), 1);
            if (!subcategory.empty()) {
                source << "Subcategory: " << subcategory << "\n";
            }
            wrote_taxonomy = true;
        }

        const std::string file_name = sqlite_text(stmt.get(), 2);
        if (!file_name.empty()) {
            source << "Example file: " << file_name << "\n";
            source << "Example path: " << sqlite_text(stmt.get(), 3) << "\n";
            source << "Example extension: " << sqlite_text(stmt.get(), 4) << "\n";
            const std::string suggested_name = sqlite_text(stmt.get(), 5);
            if (!suggested_name.empty()) {
                source << "Suggested name: " << suggested_name << "\n";
            }
            const std::string context = sqlite_text(stmt.get(), 6);
            if (!context.empty()) {
                source << "Context: " << context << "\n";
            }
        }
    }
    return wrote_taxonomy ? source.str() : std::string();
}

std::vector<UserLearningStore::TaxonomyEntry> UserLearningStore::taxonomy_entries() const
{
    std::vector<TaxonomyEntry> entries;
    if (!db_) {
        return entries;
    }

    const char* sql = R"(
        SELECT id, category, subcategory, example_count, source
        FROM learned_taxonomy_entries
        ORDER BY normalized_category, normalized_subcategory, id;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return entries;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        entries.push_back(TaxonomyEntry{
            sqlite3_column_int(stmt.get(), 0),
            sqlite_text(stmt.get(), 1),
            sqlite_text(stmt.get(), 2),
            sqlite3_column_int(stmt.get(), 3),
            sqlite_text(stmt.get(), 4)
        });
    }

    return entries;
}

int UserLearningStore::taxonomy_entry_count() const
{
    if (!db_) {
        return 0;
    }
    const char* sql = "SELECT COUNT(*) FROM learned_taxonomy_entries;";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt || sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return 0;
    }
    return sqlite3_column_int(stmt.get(), 0);
}

std::vector<UserLearningStore::ApprovedExample> UserLearningStore::approved_examples() const
{
    std::vector<ApprovedExample> examples;
    if (!db_) {
        return examples;
    }

    const char* sql = R"(
        SELECT e.id,
               e.taxonomy_entry_id,
               e.file_name,
               e.file_type,
               e.dir_path,
               t.category,
               t.subcategory,
               e.suggested_name,
               e.context_text,
               e.used_consistency_hints,
               e.source
        FROM approved_category_examples e
        JOIN learned_taxonomy_entries t ON t.id = e.taxonomy_entry_id
        ORDER BY e.id;
    )";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt) {
        return examples;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        examples.push_back(ApprovedExample{
            sqlite3_column_int(stmt.get(), 0),
            sqlite3_column_int(stmt.get(), 1),
            sqlite_text(stmt.get(), 2),
            file_type_from_db(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3))),
            sqlite_text(stmt.get(), 4),
            sqlite_text(stmt.get(), 5),
            sqlite_text(stmt.get(), 6),
            sqlite_text(stmt.get(), 7),
            sqlite_text(stmt.get(), 8),
            sqlite3_column_int(stmt.get(), 9) != 0,
            sqlite_text(stmt.get(), 10)
        });
    }

    return examples;
}

int UserLearningStore::approved_example_count() const
{
    if (!db_) {
        return 0;
    }
    const char* sql = "SELECT COUNT(*) FROM approved_category_examples;";
    auto stmt = prepare_statement(db_, sql, nullptr);
    if (!stmt || sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return 0;
    }
    return sqlite3_column_int(stmt.get(), 0);
}

std::string UserLearningStore::normalize_label(const std::string& value) const
{
    std::string collapsed;
    collapsed.reserve(value.size());
    bool previous_space = true;
    for (unsigned char ch : value) {
        if (TextEncoding::ascii_space(ch)) {
            if (!previous_space) {
                collapsed.push_back(' ');
            }
            previous_space = true;
            continue;
        }
        collapsed.push_back(static_cast<char>(TextEncoding::ascii_lower(ch)));
        previous_space = false;
    }
    while (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }
    return collapsed;
}
