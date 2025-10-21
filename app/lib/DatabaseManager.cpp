#include "DatabaseManager.hpp"
#include "Types.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include <glib.h>
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace {
constexpr double kSimilarityThreshold = 0.85;

template <typename... Args>
void db_log(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}

bool is_duplicate_column_error(const char *error_msg) {
    if (!error_msg) {
        return false;
    }
    std::string message(error_msg);
    std::transform(message.begin(), message.end(), message.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return message.find("duplicate column name") != std::string::npos;
}
} // namespace

DatabaseManager::DatabaseManager(std::string config_dir)
    : db(nullptr),
      config_dir(std::move(config_dir)),
      db_file(this->config_dir + "/" +
              (std::getenv("CATEGORIZATION_CACHE_FILE")
                   ? std::getenv("CATEGORIZATION_CACHE_FILE")
                   : "categorization_results.db")) {
    if (db_file.empty()) {
        db_log(spdlog::level::err, "Error: Database path is empty");
        return;
    }

    if (sqlite3_open(db_file.c_str(), &db) != SQLITE_OK) {
        db_log(spdlog::level::err, "Can't open database: {}", sqlite3_errmsg(db));
        db = nullptr;
        return;
    }

    sqlite3_extended_result_codes(db, 1);

    initialize_schema();
    initialize_taxonomy_schema();
    load_taxonomy_cache();
}

DatabaseManager::~DatabaseManager() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

void DatabaseManager::initialize_schema() {
    if (!db) return;

    const char *create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS file_categorization (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL,
            file_type TEXT NOT NULL,
            dir_path TEXT NOT NULL,
            category TEXT NOT NULL,
            subcategory TEXT,
            taxonomy_id INTEGER,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(file_name, file_type, dir_path)
        );
    )";

    char *error_msg = nullptr;
    if (sqlite3_exec(db, create_table_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create file_categorization table: {}", error_msg);
        sqlite3_free(error_msg);
    }

    const char *add_column_sql = "ALTER TABLE file_categorization ADD COLUMN taxonomy_id INTEGER;";
    if (sqlite3_exec(db, add_column_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        if (!is_duplicate_column_error(error_msg)) {
            db_log(spdlog::level::warn, "Failed to add taxonomy_id column: {}", error_msg ? error_msg : "");
        }
        if (error_msg) {
            sqlite3_free(error_msg);
        }
    }

    const char *create_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_file_categorization_taxonomy ON file_categorization(taxonomy_id);";
    if (sqlite3_exec(db, create_index_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create taxonomy index: {}", error_msg);
        sqlite3_free(error_msg);
    }
}

void DatabaseManager::initialize_taxonomy_schema() {
    if (!db) return;

    const char *taxonomy_sql = R"(
        CREATE TABLE IF NOT EXISTS category_taxonomy (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            canonical_category TEXT NOT NULL,
            canonical_subcategory TEXT NOT NULL,
            normalized_category TEXT NOT NULL,
            normalized_subcategory TEXT NOT NULL,
            frequency INTEGER DEFAULT 0,
            UNIQUE(normalized_category, normalized_subcategory)
        );
    )";

    char *error_msg = nullptr;
    if (sqlite3_exec(db, taxonomy_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create category_taxonomy table: {}", error_msg);
        sqlite3_free(error_msg);
    }

    const char *alias_sql = R"(
        CREATE TABLE IF NOT EXISTS category_alias (
            alias_category_norm TEXT NOT NULL,
            alias_subcategory_norm TEXT NOT NULL,
            taxonomy_id INTEGER NOT NULL,
            PRIMARY KEY(alias_category_norm, alias_subcategory_norm),
            FOREIGN KEY(taxonomy_id) REFERENCES category_taxonomy(id)
        );
    )";
    if (sqlite3_exec(db, alias_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create category_alias table: {}", error_msg);
        sqlite3_free(error_msg);
    }

    const char *alias_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_category_alias_taxonomy ON category_alias(taxonomy_id);";
    if (sqlite3_exec(db, alias_index_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create alias index: {}", error_msg);
        sqlite3_free(error_msg);
    }
}

void DatabaseManager::load_taxonomy_cache() {
    taxonomy_entries.clear();
    canonical_lookup.clear();
    alias_lookup.clear();
    taxonomy_index.clear();

    if (!db) return;

    sqlite3_stmt *stmt = nullptr;
    const char *select_taxonomy =
        "SELECT id, canonical_category, canonical_subcategory, "
        "normalized_category, normalized_subcategory, frequency FROM category_taxonomy;";

    if (sqlite3_prepare_v2(db, select_taxonomy, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TaxonomyEntry entry;
            entry.id = sqlite3_column_int(stmt, 0);
            entry.category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            entry.subcategory = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            entry.normalized_category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            entry.normalized_subcategory = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));

            taxonomy_index[entry.id] = taxonomy_entries.size();
            taxonomy_entries.push_back(entry);
            canonical_lookup[make_key(entry.normalized_category, entry.normalized_subcategory)] = entry.id;
        }
    } else {
        db_log(spdlog::level::err, "Failed to load taxonomy cache: {}", sqlite3_errmsg(db));
    }
    if (stmt) sqlite3_finalize(stmt);

    const char *select_alias =
        "SELECT alias_category_norm, alias_subcategory_norm, taxonomy_id FROM category_alias;";
    if (sqlite3_prepare_v2(db, select_alias, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string alias_cat = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::string alias_subcat = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            int taxonomy_id = sqlite3_column_int(stmt, 2);

            alias_lookup[make_key(alias_cat, alias_subcat)] = taxonomy_id;
        }
    } else {
        db_log(spdlog::level::err, "Failed to load category aliases: {}", sqlite3_errmsg(db));
    }
    if (stmt) sqlite3_finalize(stmt);
}

std::string DatabaseManager::normalize_label(const std::string &input) const {
    std::string result;
    result.reserve(input.size());

    bool last_was_space = true;
    for (unsigned char ch : input) {
        if (std::isalnum(ch)) {
            result.push_back(static_cast<char>(std::tolower(ch)));
            last_was_space = false;
        } else if (std::isspace(ch)) {
            if (!last_was_space) {
                result.push_back(' ');
                last_was_space = true;
            }
        }
    }

    // Trim leading/trailing space if any
    while (!result.empty() && result.front() == ' ') {
        result.erase(result.begin());
    }
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

double DatabaseManager::string_similarity(const std::string &a, const std::string &b) {
    if (a == b) {
        return 1.0;
    }
    if (a.empty() || b.empty()) {
        return 0.0;
    }

    const size_t m = a.size();
    const size_t n = b.size();
    std::vector<size_t> prev(n + 1), curr(n + 1);

    for (size_t j = 0; j <= n; ++j) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= m; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }

    const double dist = static_cast<double>(prev[n]);
    const double max_len = static_cast<double>(std::max(m, n));
    return 1.0 - (dist / max_len);
}

std::string DatabaseManager::make_key(const std::string &norm_category,
                                      const std::string &norm_subcategory) {
    return norm_category + "::" + norm_subcategory;
}

int DatabaseManager::create_taxonomy_entry(const std::string &category,
                                           const std::string &subcategory,
                                           const std::string &norm_category,
                                           const std::string &norm_subcategory) {
    if (!db) return -1;

    const char *sql = R"(
        INSERT INTO category_taxonomy
            (canonical_category, canonical_subcategory, normalized_category, normalized_subcategory, frequency)
        VALUES (?, ?, ?, ?, 0);
    )";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare taxonomy insert: {}", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, subcategory.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, norm_category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, norm_subcategory.c_str(), -1, SQLITE_TRANSIENT);

    int step_rc = sqlite3_step(stmt);
    int extended_rc = sqlite3_extended_errcode(db);
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) {
        if (extended_rc == SQLITE_CONSTRAINT_UNIQUE ||
            extended_rc == SQLITE_CONSTRAINT_PRIMARYKEY ||
            extended_rc == SQLITE_CONSTRAINT) {
            return find_existing_taxonomy_id(norm_category, norm_subcategory);
        }

        db_log(spdlog::level::err, "Failed to insert taxonomy entry: {}", sqlite3_errmsg(db));
        return -1;
    }

    int new_id = static_cast<int>(sqlite3_last_insert_rowid(db));
    TaxonomyEntry entry{new_id, category, subcategory, norm_category, norm_subcategory};
    taxonomy_index[new_id] = taxonomy_entries.size();
    taxonomy_entries.push_back(entry);
    canonical_lookup[make_key(norm_category, norm_subcategory)] = new_id;
    return new_id;
}

int DatabaseManager::find_existing_taxonomy_id(const std::string &norm_category,
                                               const std::string &norm_subcategory) const {
    if (!db) return -1;

    const char *select_sql =
        "SELECT id FROM category_taxonomy WHERE normalized_category = ? AND normalized_subcategory = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    int existing_id = -1;

    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, norm_category.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, norm_subcategory.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            existing_id = sqlite3_column_int(stmt, 0);
        }
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    return existing_id;
}

void DatabaseManager::ensure_alias_mapping(int taxonomy_id,
                                           const std::string &norm_category,
                                           const std::string &norm_subcategory) {
    if (!db) return;

    std::string key = make_key(norm_category, norm_subcategory);

    auto canonical_it = canonical_lookup.find(key);
    if (canonical_it != canonical_lookup.end() && canonical_it->second == taxonomy_id) {
        return; // Already canonical form
    }

    if (alias_lookup.find(key) != alias_lookup.end()) {
        return;
    }

    const char *sql = R"(
        INSERT OR IGNORE INTO category_alias (alias_category_norm, alias_subcategory_norm, taxonomy_id)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare alias insert: {}", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_text(stmt, 1, norm_category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, norm_subcategory.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, taxonomy_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        db_log(spdlog::level::err, "Failed to insert alias: {}", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_finalize(stmt);
    alias_lookup[key] = taxonomy_id;
}

const DatabaseManager::TaxonomyEntry *DatabaseManager::find_taxonomy_entry(int taxonomy_id) const {
    auto it = taxonomy_index.find(taxonomy_id);
    if (it == taxonomy_index.end()) {
        return nullptr;
    }
    size_t idx = it->second;
    if (idx >= taxonomy_entries.size()) {
        return nullptr;
    }
    return &taxonomy_entries[idx];
}

std::pair<int, double> DatabaseManager::find_fuzzy_match(
    const std::string& norm_category,
    const std::string& norm_subcategory) const {
    if (taxonomy_entries.empty()) {
        return {-1, 0.0};
    }

    double best_score = 0.0;
    int best_id = -1;
    for (const auto &entry : taxonomy_entries) {
        double category_score = string_similarity(norm_category, entry.normalized_category);
        double subcategory_score =
            string_similarity(norm_subcategory, entry.normalized_subcategory);
        double combined = (category_score + subcategory_score) / 2.0;
        if (combined > best_score) {
            best_score = combined;
            best_id = entry.id;
        }
    }

    if (best_id != -1 && best_score >= kSimilarityThreshold) {
        return {best_id, best_score};
    }
    return {-1, best_score};
}

int DatabaseManager::resolve_existing_taxonomy(const std::string& key,
                                               const std::string& norm_category,
                                               const std::string& norm_subcategory) const {
    auto alias_it = alias_lookup.find(key);
    if (alias_it != alias_lookup.end()) {
        return alias_it->second;
    }

    auto canonical_it = canonical_lookup.find(key);
    if (canonical_it != canonical_lookup.end()) {
        return canonical_it->second;
    }

    auto [best_id, score] = find_fuzzy_match(norm_category, norm_subcategory);
    return best_id;
}

DatabaseManager::ResolvedCategory DatabaseManager::build_resolved_category(
    int taxonomy_id,
    const std::string& fallback_category,
    const std::string& fallback_subcategory,
    const std::string& norm_category,
    const std::string& norm_subcategory) {

    ResolvedCategory result{-1, fallback_category, fallback_subcategory};

    if (taxonomy_id == -1) {
        taxonomy_id = create_taxonomy_entry(fallback_category, fallback_subcategory,
                                            norm_category, norm_subcategory);
    }

    if (taxonomy_id != -1) {
        ensure_alias_mapping(taxonomy_id, norm_category, norm_subcategory);
        if (const auto *entry = find_taxonomy_entry(taxonomy_id)) {
            result.taxonomy_id = entry->id;
            result.category = entry->category;
            result.subcategory = entry->subcategory;
        } else {
            result.taxonomy_id = taxonomy_id;
        }
    } else {
        result.category = fallback_category;
        result.subcategory = fallback_subcategory;
    }

    return result;
}

DatabaseManager::ResolvedCategory
DatabaseManager::resolve_category(const std::string &category,
                                  const std::string &subcategory) {
    ResolvedCategory result{-1, category, subcategory};
    if (!db) {
        return result;
    }

    auto trim_copy = [](std::string value) {
        auto is_space = [](unsigned char ch) { return std::isspace(ch); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                                [&](unsigned char ch) { return !is_space(ch); }));
        value.erase(std::find_if(value.rbegin(), value.rend(),
                                 [&](unsigned char ch) { return !is_space(ch); }).base(),
                    value.end());
        return value;
    };

    std::string trimmed_category = trim_copy(category);
    std::string trimmed_subcategory = trim_copy(subcategory);

    if (trimmed_category.empty()) {
        trimmed_category = "Uncategorized";
    }
    if (trimmed_subcategory.empty()) {
        trimmed_subcategory = "General";
    }

    std::string norm_category = normalize_label(trimmed_category);
    std::string norm_subcategory = normalize_label(trimmed_subcategory);
    std::string key = make_key(norm_category, norm_subcategory);

    int taxonomy_id = resolve_existing_taxonomy(key, norm_category, norm_subcategory);
    return build_resolved_category(taxonomy_id,
                                   trimmed_category,
                                   trimmed_subcategory,
                                   norm_category,
                                   norm_subcategory);
}

bool DatabaseManager::insert_or_update_file_with_categorization(
    const std::string &file_name,
    const std::string &file_type,
    const std::string &dir_path,
    const ResolvedCategory &resolved) {
    if (!db) return false;

    const char *sql = R"(
        INSERT INTO file_categorization
            (file_name, file_type, dir_path, category, subcategory, taxonomy_id)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(file_name, file_type, dir_path)
        DO UPDATE SET
            category = excluded.category,
            subcategory = excluded.subcategory,
            taxonomy_id = excluded.taxonomy_id;
    )";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "SQL prepare error: {}", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, dir_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, resolved.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, resolved.subcategory.c_str(), -1, SQLITE_TRANSIENT);

    if (resolved.taxonomy_id > 0) {
        sqlite3_bind_int(stmt, 6, resolved.taxonomy_id);
    } else {
        sqlite3_bind_null(stmt, 6);
    }

    bool success = true;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        db_log(spdlog::level::err, "SQL error during insert/update: {}", sqlite3_errmsg(db));
        success = false;
    }

    sqlite3_finalize(stmt);

    if (success && resolved.taxonomy_id > 0) {
        increment_taxonomy_frequency(resolved.taxonomy_id);
    }

    return success;
}

void DatabaseManager::increment_taxonomy_frequency(int taxonomy_id) {
    if (!db || taxonomy_id <= 0) return;

    const char *sql =
        "UPDATE category_taxonomy "
        "SET frequency = (SELECT COUNT(*) FROM file_categorization WHERE taxonomy_id = ?) "
        "WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare frequency update: {}", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_int(stmt, 1, taxonomy_id);
    sqlite3_bind_int(stmt, 2, taxonomy_id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        db_log(spdlog::level::err, "Failed to increment taxonomy frequency: {}", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

std::vector<CategorizedFile>
DatabaseManager::get_categorized_files(const std::string &directory_path) {
    std::vector<CategorizedFile> categorized_files;
    if (!db) return categorized_files;

    const char *sql =
        "SELECT dir_path, file_name, file_type, category, subcategory, taxonomy_id "
        "FROM file_categorization WHERE dir_path = ?;";
    sqlite3_stmt *stmtcat = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmtcat, nullptr) != SQLITE_OK) {
        return categorized_files;
    }

    if (sqlite3_bind_text(stmtcat, 1, directory_path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to bind directory_path: {}", sqlite3_errmsg(db));
        sqlite3_finalize(stmtcat);
        return categorized_files;
    }

    while (sqlite3_step(stmtcat) == SQLITE_ROW) {
        const char *file_dir_path = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 0));
        const char *file_name = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 1));
        const char *file_type = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 2));
        const char *category = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 3));
        const char *subcategory = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 4));

        std::string dir_path = file_dir_path ? file_dir_path : "";
        std::string name = file_name ? file_name : "";
        std::string type_str = file_type ? file_type : "";
        std::string cat = category ? category : "";
        std::string subcat = subcategory ? subcategory : "";

        int taxonomy_id = 0;
        if (sqlite3_column_type(stmtcat, 5) != SQLITE_NULL) {
            taxonomy_id = sqlite3_column_int(stmtcat, 5);
        }

        FileType file_type_enum = (type_str == "F") ? FileType::File : FileType::Directory;
        categorized_files.push_back({dir_path, name, file_type_enum, cat, subcat, taxonomy_id});
    }

    sqlite3_finalize(stmtcat);
    return categorized_files;
}

std::vector<std::string>
DatabaseManager::get_categorization_from_db(const std::string &file_name, const FileType file_type) {
    std::vector<std::string> categorization;
    if (!db) return categorization;

    const char *sql =
        "SELECT category, subcategory FROM file_categorization WHERE file_name = ? AND file_type = ?;";
    sqlite3_stmt *stmtcat = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmtcat, nullptr) != SQLITE_OK) {
        return categorization;
    }

    if (sqlite3_bind_text(stmtcat, 1, file_name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmtcat);
        return categorization;
    }

    std::string file_type_str = (file_type == FileType::File) ? "F" : "D";
    if (sqlite3_bind_text(stmtcat, 2, file_type_str.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmtcat);
        return categorization;
    }

    if (sqlite3_step(stmtcat) == SQLITE_ROW) {
        const char *category = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 0));
        const char *subcategory = reinterpret_cast<const char *>(sqlite3_column_text(stmtcat, 1));
        categorization.emplace_back(category ? category : "");
        categorization.emplace_back(subcategory ? subcategory : "");
    }

    sqlite3_finalize(stmtcat);
    return categorization;
}

bool DatabaseManager::is_file_already_categorized(const std::string &file_name) {
    if (!db) return false;

    const char *sql = "SELECT 1 FROM file_categorization WHERE file_name = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, file_name.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

std::vector<std::string> DatabaseManager::get_dir_contents_from_db(const std::string &dir_path) {
    std::vector<std::string> results;
    if (!db) return results;

    const char *sql = "SELECT file_name FROM file_categorization WHERE dir_path = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        results.emplace_back(name ? name : "");
    }
    sqlite3_finalize(stmt);
    return results;
}

std::string DatabaseManager::get_cached_category(const std::string &file_name) {
    auto iter = cached_results.find(file_name);
    if (iter != cached_results.end()) {
        return iter->second;
    }
    return {};
}

void DatabaseManager::load_cache() {
    cached_results.clear();
}

bool DatabaseManager::file_exists_in_db(const std::string &file_name, const std::string &file_path) {
    if (!db) return false;

    const char *sql =
        "SELECT 1 FROM file_categorization WHERE file_name = ? AND dir_path = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file_path.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}
