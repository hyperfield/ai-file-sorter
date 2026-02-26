#include "DatabaseManager.hpp"
#include "Types.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

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

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string extract_extension_lower(const std::string& file_name) {
    const auto pos = file_name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= file_name.size()) {
        return std::string();
    }
    std::string ext = file_name.substr(pos);
    return to_lower_copy(ext);
}

struct StatementDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

StatementPtr prepare_statement(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
        return StatementPtr{};
    }
    return StatementPtr(raw);
}

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool has_label_content(const std::string& value) {
    return !trim_copy(value).empty();
}

std::string escape_like_pattern(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);
    for (char ch : value) {
        if (ch == '\\' || ch == '%' || ch == '_') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string build_recursive_dir_pattern(const std::string& directory_path) {
    std::string escaped = escape_like_pattern(directory_path);
    if (directory_path.empty()) {
        return escaped + "%";
    }
    const char sep = directory_path.find('\\') != std::string::npos ? '\\' : '/';
    if (directory_path.back() == sep) {
        escaped.push_back('%');
        return escaped;
    }
    if (sep == '\\' || sep == '%' || sep == '_') {
        escaped.push_back('\\');
    }
    escaped.push_back(sep);
    escaped.push_back('%');
    return escaped;
}

std::optional<CategorizedFile> build_categorized_entry(sqlite3_stmt* stmt) {
    const char *file_dir_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    const char *file_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    const char *file_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    const char *category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    const char *subcategory = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    const char *suggested_name = nullptr;
    if (sqlite3_column_count(stmt) > 5) {
        suggested_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    }

    std::string dir_path = file_dir_path ? file_dir_path : "";
    std::string name = file_name ? file_name : "";
    std::string type_str = file_type ? file_type : "";
    std::string cat = category ? category : "";
    std::string subcat = subcategory ? subcategory : "";
    std::string suggested = suggested_name ? suggested_name : "";

    int taxonomy_id = 0;
    if (sqlite3_column_count(stmt) > 6 && sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        taxonomy_id = sqlite3_column_int(stmt, 6);
    }
    bool used_consistency = false;
    if (sqlite3_column_count(stmt) > 7 && sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
        used_consistency = sqlite3_column_int(stmt, 7) != 0;
    }
    bool rename_only = false;
    if (sqlite3_column_count(stmt) > 8 && sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
        rename_only = sqlite3_column_int(stmt, 8) != 0;
    }
    bool rename_applied = false;
    if (sqlite3_column_count(stmt) > 9 && sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        rename_applied = sqlite3_column_int(stmt, 9) != 0;
    }

    const bool has_labels = has_label_content(cat) && has_label_content(subcat);
    const bool has_suggestion = has_label_content(suggested);
    if (!rename_only && !has_labels && !has_suggestion) {
        return std::nullopt;
    }

    FileType file_type_enum = (type_str == "F") ? FileType::File : FileType::Directory;
    CategorizedFile entry{dir_path, name, file_type_enum, cat, subcat, taxonomy_id};
    entry.from_cache = true;
    entry.used_consistency_hints = used_consistency;
    entry.suggested_name = suggested;
    entry.rename_only = rename_only;
    entry.rename_applied = rename_applied;
    return entry;
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
            suggested_name TEXT,
            taxonomy_id INTEGER,
            categorization_style INTEGER DEFAULT 0,
            rename_only INTEGER DEFAULT 0,
            rename_applied INTEGER DEFAULT 0,
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

    const char *add_style_column_sql =
        "ALTER TABLE file_categorization ADD COLUMN categorization_style INTEGER DEFAULT 0;";
    if (sqlite3_exec(db, add_style_column_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        if (!is_duplicate_column_error(error_msg)) {
            db_log(spdlog::level::warn, "Failed to add categorization_style column: {}", error_msg ? error_msg : "");
        }
        if (error_msg) {
            sqlite3_free(error_msg);
        }
    }

    const char *add_suggested_name_column_sql =
        "ALTER TABLE file_categorization ADD COLUMN suggested_name TEXT;";
    if (sqlite3_exec(db, add_suggested_name_column_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        if (!is_duplicate_column_error(error_msg)) {
            db_log(spdlog::level::warn, "Failed to add suggested_name column: {}", error_msg ? error_msg : "");
        }
        if (error_msg) {
            sqlite3_free(error_msg);
        }
    }

    const char *add_rename_only_column_sql =
        "ALTER TABLE file_categorization ADD COLUMN rename_only INTEGER DEFAULT 0;";
    if (sqlite3_exec(db, add_rename_only_column_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        if (!is_duplicate_column_error(error_msg)) {
            db_log(spdlog::level::warn, "Failed to add rename_only column: {}", error_msg ? error_msg : "");
        }
        if (error_msg) {
            sqlite3_free(error_msg);
        }
    }

    const char *add_rename_applied_column_sql =
        "ALTER TABLE file_categorization ADD COLUMN rename_applied INTEGER DEFAULT 0;";
    if (sqlite3_exec(db, add_rename_applied_column_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        if (!is_duplicate_column_error(error_msg)) {
            db_log(spdlog::level::warn, "Failed to add rename_applied column: {}", error_msg ? error_msg : "");
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

static std::string strip_trailing_stopwords(const std::string& normalized) {
    if (normalized.empty()) {
        return normalized;
    }
    static const std::unordered_set<std::string> kStopwords = {
        "file", "files",
        "doc", "docs", "document", "documents",
        "image", "images",
        "photo", "photos",
        "pic", "pics"
    };

    std::istringstream iss(normalized);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() <= 1) {
        return normalized;
    }
    while (tokens.size() > 1 && kStopwords.contains(tokens.back())) {
        tokens.pop_back();
    }
    if (tokens.empty()) {
        return normalized;
    }

    std::string joined;
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (index > 0) {
            joined.push_back(' ');
        }
        joined += tokens[index];
    }
    return joined;
}

struct CanonicalCategoryLabel {
    std::string normalized;
    std::string display;
};

bool is_image_like_label(const std::string& normalized) {
    if (normalized.empty()) {
        return false;
    }
    static const std::unordered_set<std::string> kImageLike = {
        "image", "images",
        "image file", "image files",
        "photo", "photos",
        "graphic", "graphics",
        "picture", "pictures",
        "pic", "pics",
        "screenshot", "screenshots",
        "wallpaper", "wallpapers"
    };
    if (kImageLike.contains(normalized)) {
        return true;
    }
    return kImageLike.contains(strip_trailing_stopwords(normalized));
}

CanonicalCategoryLabel canonicalize_category_label(const std::string& normalized_category,
                                                   const std::string& normalized_subcategory) {
    static const std::unordered_map<std::string, CanonicalCategoryLabel> kCategorySynonyms = {
        {"archive", {"archives", "Archives"}},
        {"archives", {"archives", "Archives"}},
        {"backup", {"archives", "Archives"}},
        {"backups", {"archives", "Archives"}},
        {"backup file", {"archives", "Archives"}},
        {"backup files", {"archives", "Archives"}},

        {"document", {"documents", "Documents"}},
        {"documents", {"documents", "Documents"}},
        {"doc", {"documents", "Documents"}},
        {"docs", {"documents", "Documents"}},
        {"text", {"documents", "Documents"}},
        {"texts", {"documents", "Documents"}},
        {"paper", {"documents", "Documents"}},
        {"papers", {"documents", "Documents"}},
        {"report", {"documents", "Documents"}},
        {"reports", {"documents", "Documents"}},
        {"spreadsheet", {"documents", "Documents"}},
        {"spreadsheets", {"documents", "Documents"}},
        {"table", {"documents", "Documents"}},
        {"tables", {"documents", "Documents"}},
        {"office file", {"documents", "Documents"}},
        {"office files", {"documents", "Documents"}},

        {"software", {"software", "Software"}},
        {"application", {"software", "Software"}},
        {"applications", {"software", "Software"}},
        {"app", {"software", "Software"}},
        {"apps", {"software", "Software"}},
        {"program", {"software", "Software"}},
        {"programs", {"software", "Software"}},
        {"installer", {"software", "Software"}},
        {"installers", {"software", "Software"}},
        {"installation", {"software", "Software"}},
        {"installations", {"software", "Software"}},
        {"installation file", {"software", "Software"}},
        {"installation files", {"software", "Software"}},
        {"software installation", {"software", "Software"}},
        {"software installations", {"software", "Software"}},
        {"software installation file", {"software", "Software"}},
        {"software installation files", {"software", "Software"}},
        {"setup", {"software", "Software"}},
        {"setups", {"software", "Software"}},
        {"setup file", {"software", "Software"}},
        {"setup files", {"software", "Software"}},
        {"update", {"software", "Software"}},
        {"updates", {"software", "Software"}},
        {"software update", {"software", "Software"}},
        {"software updates", {"software", "Software"}},
        {"patch", {"software", "Software"}},
        {"patches", {"software", "Software"}},
        {"upgrade", {"software", "Software"}},
        {"upgrades", {"software", "Software"}},
        {"updater", {"software", "Software"}},
        {"updaters", {"software", "Software"}},

        {"image", {"images", "Images"}},
        {"images", {"images", "Images"}},
        {"image file", {"images", "Images"}},
        {"image files", {"images", "Images"}},
        {"photo", {"images", "Images"}},
        {"photos", {"images", "Images"}},
        {"graphic", {"images", "Images"}},
        {"graphics", {"images", "Images"}},
        {"picture", {"images", "Images"}},
        {"pictures", {"images", "Images"}},
        {"pic", {"images", "Images"}},
        {"pics", {"images", "Images"}},
        {"screenshot", {"images", "Images"}},
        {"screenshots", {"images", "Images"}},
        {"wallpaper", {"images", "Images"}},
        {"wallpapers", {"images", "Images"}}
    };

    if (auto it = kCategorySynonyms.find(normalized_category); it != kCategorySynonyms.end()) {
        return it->second;
    }

    const std::string stripped_category = strip_trailing_stopwords(normalized_category);
    if (auto it = kCategorySynonyms.find(stripped_category); it != kCategorySynonyms.end()) {
        return it->second;
    }

    // "Media" can be broader than images, so only collapse when the paired subcategory is image-like.
    if ((normalized_category == "media" || stripped_category == "media") &&
        is_image_like_label(normalized_subcategory)) {
        return {"images", "Images"};
    }

    return {normalized_category, ""};
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
    const CanonicalCategoryLabel canonical_category = canonicalize_category_label(norm_category, norm_subcategory);
    norm_category = canonical_category.normalized;
    if (!canonical_category.display.empty()) {
        trimmed_category = canonical_category.display;
    }
    const std::string match_subcategory = strip_trailing_stopwords(norm_subcategory);
    std::string key = make_key(norm_category, match_subcategory);

    int taxonomy_id = resolve_existing_taxonomy(key, norm_category, match_subcategory);
    if (taxonomy_id == -1 && match_subcategory != norm_subcategory) {
        const std::string raw_key = make_key(norm_category, norm_subcategory);
        taxonomy_id = resolve_existing_taxonomy(raw_key, norm_category, norm_subcategory);
    }
    return build_resolved_category(taxonomy_id,
                                   trimmed_category,
                                   trimmed_subcategory,
                                   norm_category,
                                   match_subcategory);
}

bool DatabaseManager::insert_or_update_file_with_categorization(
    const std::string &file_name,
    const std::string &file_type,
    const std::string &dir_path,
    const ResolvedCategory &resolved,
    bool used_consistency_hints,
    const std::string &suggested_name,
    bool rename_only,
    bool rename_applied) {
    if (!db) return false;

    const char *sql = R"(
        INSERT INTO file_categorization
            (file_name, file_type, dir_path, category, subcategory, suggested_name,
             taxonomy_id, categorization_style, rename_only, rename_applied)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(file_name, file_type, dir_path)
        DO UPDATE SET
            category = excluded.category,
            subcategory = excluded.subcategory,
            suggested_name = excluded.suggested_name,
            taxonomy_id = excluded.taxonomy_id,
            categorization_style = excluded.categorization_style,
            rename_only = excluded.rename_only,
            rename_applied = CASE
                WHEN excluded.rename_applied = 1 THEN 1
                ELSE rename_applied
            END;
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
    sqlite3_bind_text(stmt, 6, suggested_name.c_str(), -1, SQLITE_TRANSIENT);

    if (resolved.taxonomy_id > 0) {
        sqlite3_bind_int(stmt, 7, resolved.taxonomy_id);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    sqlite3_bind_int(stmt, 8, used_consistency_hints ? 1 : 0);
    sqlite3_bind_int(stmt, 9, rename_only ? 1 : 0);
    sqlite3_bind_int(stmt, 10, rename_applied ? 1 : 0);

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

bool DatabaseManager::remove_file_categorization(const std::string& dir_path,
                                                 const std::string& file_name,
                                                 const FileType file_type) {
    if (!db) {
        return false;
    }

    const char* sql =
        "DELETE FROM file_categorization WHERE dir_path = ? AND file_name = ? AND file_type = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare delete categorization statement: {}", sqlite3_errmsg(db));
        return false;
    }

    const std::string type_str = (file_type == FileType::File) ? "F" : "D";

    sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type_str.c_str(), -1, SQLITE_TRANSIENT);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        db_log(spdlog::level::err, "Failed to delete cached categorization for '{}': {}", file_name, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::clear_directory_categorizations(const std::string& dir_path) {
    if (!db) {
        return false;
    }

    const char* sql = "DELETE FROM file_categorization WHERE dir_path = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare directory cache clear statement: {}", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        db_log(spdlog::level::err, "Failed to clear cached categorizations for '{}': {}", dir_path, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    cached_results.clear();
    return success;
}

std::optional<bool> DatabaseManager::get_directory_categorization_style(const std::string& dir_path) const {
    if (!db) {
        return std::nullopt;
    }

    const char* sql =
        "SELECT categorization_style FROM file_categorization WHERE dir_path = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::warn, "Failed to prepare cached style query: {}", sqlite3_errmsg(db));
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<bool> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // If the column exists but is NULL (older rows), treat as "false" (refined) to compare
        // against the user's current preference.
        result = (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
                     ? (sqlite3_column_int(stmt, 0) != 0)
                     : false;
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<CategorizedFile>
DatabaseManager::remove_empty_categorizations(const std::string& dir_path) {
    std::vector<CategorizedFile> removed;
    if (!db) {
        return removed;
    }

    const char* sql = R"(
        SELECT file_name, file_type, IFNULL(category, ''), IFNULL(subcategory, ''), taxonomy_id
        FROM file_categorization
        WHERE dir_path = ?
          AND (category IS NULL OR TRIM(category) = '' OR subcategory IS NULL OR TRIM(subcategory) = '')
          AND (suggested_name IS NULL OR TRIM(suggested_name) = '')
          AND IFNULL(rename_only, 0) = 0;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare empty categorization query: {}", sqlite3_errmsg(db));
        return removed;
    }

    if (sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to bind directory path for empty categorization query: {}", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return removed;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* subcategory = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        std::string file_name = name ? name : "";
        std::string type_str = type ? type : "";
        FileType entry_type = (type_str == "D") ? FileType::Directory : FileType::File;

        int taxonomy_id = 0;
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            taxonomy_id = sqlite3_column_int(stmt, 4);
        }

        removed.push_back({dir_path,
                           file_name,
                           entry_type,
                           category ? category : "",
                           subcategory ? subcategory : "",
                           taxonomy_id});
    }

    sqlite3_finalize(stmt);
    for (const auto& entry : removed) {
        remove_file_categorization(entry.file_path, entry.file_name, entry.type);
    }
    return removed;
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
        "SELECT dir_path, file_name, file_type, category, subcategory, suggested_name, taxonomy_id, "
        "categorization_style, rename_only, rename_applied "
        "FROM file_categorization WHERE dir_path = ?;";
    StatementPtr stmt = prepare_statement(db, sql);
    if (!stmt) {
        return categorized_files;
    }

    if (sqlite3_bind_text(stmt.get(), 1, directory_path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to bind directory_path: {}", sqlite3_errmsg(db));
        return categorized_files;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        if (auto entry = build_categorized_entry(stmt.get())) {
            categorized_files.push_back(std::move(*entry));
        }
    }

    return categorized_files;
}

std::vector<CategorizedFile>
DatabaseManager::get_categorized_files_recursive(const std::string& directory_path) {
    std::vector<CategorizedFile> categorized_files;
    if (!db) {
        return categorized_files;
    }

    const char* sql =
        "SELECT dir_path, file_name, file_type, category, subcategory, suggested_name, taxonomy_id, "
        "categorization_style, rename_only, rename_applied "
        "FROM file_categorization "
        "WHERE dir_path = ? OR dir_path LIKE ? ESCAPE '\\';";
    StatementPtr stmt = prepare_statement(db, sql);
    if (!stmt) {
        return categorized_files;
    }

    if (sqlite3_bind_text(stmt.get(), 1, directory_path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to bind directory_path: {}", sqlite3_errmsg(db));
        return categorized_files;
    }

    const std::string pattern = build_recursive_dir_pattern(directory_path);
    if (sqlite3_bind_text(stmt.get(), 2, pattern.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to bind recursive directory pattern: {}", sqlite3_errmsg(db));
        return categorized_files;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        if (auto entry = build_categorized_entry(stmt.get())) {
            categorized_files.push_back(std::move(*entry));
        }
    }

    return categorized_files;
}

std::optional<CategorizedFile>
DatabaseManager::get_categorized_file(const std::string& dir_path,
                                      const std::string& file_name,
                                      FileType file_type) {
    if (!db) {
        return std::nullopt;
    }

    const char *sql =
        "SELECT dir_path, file_name, file_type, category, subcategory, suggested_name, taxonomy_id, "
        "categorization_style, rename_only, rename_applied "
        "FROM file_categorization "
        "WHERE dir_path = ? AND file_name = ? AND file_type = ? "
        "LIMIT 1;";
    StatementPtr stmt = prepare_statement(db, sql);
    if (!stmt) {
        return std::nullopt;
    }

    if (sqlite3_bind_text(stmt.get(), 1, dir_path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        return std::nullopt;
    }
    if (sqlite3_bind_text(stmt.get(), 2, file_name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        return std::nullopt;
    }
    const std::string type_str = (file_type == FileType::File) ? "F" : "D";
    if (sqlite3_bind_text(stmt.get(), 3, type_str.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        return std::nullopt;
    }

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return build_categorized_entry(stmt.get());
    }

    return std::nullopt;
}

std::vector<std::string>
DatabaseManager::get_categorization_from_db(const std::string& dir_path,
                                            const std::string& file_name,
                                            FileType file_type) {
    std::vector<std::string> categorization;
    if (!db) return categorization;

    const char *sql =
        "SELECT category, subcategory FROM file_categorization "
        "WHERE dir_path = ? AND file_name = ? AND file_type = ?;";
    sqlite3_stmt *stmtcat = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmtcat, nullptr) != SQLITE_OK) {
        return categorization;
    }

    if (sqlite3_bind_text(stmtcat, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmtcat);
        return categorization;
    }

    std::string file_type_str = (file_type == FileType::File) ? "F" : "D";
    if (sqlite3_bind_text(stmtcat, 2, file_name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmtcat);
        return categorization;
    }
    if (sqlite3_bind_text(stmtcat, 3, file_type_str.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
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

std::vector<std::pair<std::string, std::string>> DatabaseManager::get_taxonomy_snapshot(std::size_t max_entries) const
{
    std::vector<std::pair<std::string, std::string>> snapshot;
    if (max_entries == 0) {
        max_entries = taxonomy_entries.size();
    }
    snapshot.reserve(std::min(max_entries, taxonomy_entries.size()));
    for (const auto& entry : taxonomy_entries) {
        if (snapshot.size() >= max_entries) {
            break;
        }
        snapshot.emplace_back(entry.category, entry.subcategory);
    }
    return snapshot;
}

bool DatabaseManager::is_duplicate_category(
    const std::vector<std::pair<std::string, std::string>>& results,
    const std::pair<std::string, std::string>& candidate)
{
    return std::any_of(results.begin(), results.end(), [&candidate](const auto& existing) {
        return existing.first == candidate.first && existing.second == candidate.second;
    });
}

std::optional<std::pair<std::string, std::string>> DatabaseManager::build_recent_category_candidate(
    const char* file_name_text,
    const char* category_text,
    const char* subcategory_text,
    const std::string& normalized_extension,
    bool has_extension) const
{
    std::string file_name = file_name_text ? file_name_text : "";
    if (file_name.empty()) {
        return std::nullopt;
    }

    const std::string candidate_extension = extract_extension_lower(file_name);
    if (has_extension) {
        if (candidate_extension != normalized_extension) {
            return std::nullopt;
        }
    } else if (!candidate_extension.empty()) {
        return std::nullopt;
    }

    std::string category = category_text ? category_text : "";
    if (category.empty()) {
        return std::nullopt;
    }

    std::string subcategory = subcategory_text ? subcategory_text : "";
    return std::make_pair(std::move(category), std::move(subcategory));
}

std::vector<std::pair<std::string, std::string>>
DatabaseManager::get_recent_categories_for_extension(const std::string& extension,
                                                     FileType file_type,
                                                     std::size_t limit) const
{
    std::vector<std::pair<std::string, std::string>> results;
    if (!db || limit == 0) {
        return results;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT file_name, category, subcategory FROM file_categorization "
        "WHERE file_type = ? ORDER BY timestamp DESC LIMIT ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::warn,
               "Failed to prepare recent category lookup: {}",
               sqlite3_errmsg(db));
        return results;
    }

    const std::string type_code(1, file_type == FileType::File ? 'F' : 'D');
    sqlite3_bind_text(stmt, 1, type_code.c_str(), -1, SQLITE_TRANSIENT);
    const std::size_t fetch_limit = std::max<std::size_t>(limit * 5, limit);
    sqlite3_bind_int(stmt, 2, static_cast<int>(fetch_limit));

    const std::string normalized_extension = to_lower_copy(extension);
    const bool has_extension = !normalized_extension.empty();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* file_name_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* category_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* subcategory_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        const auto candidate = build_recent_category_candidate(file_name_text,
                                                               category_text,
                                                               subcategory_text,
                                                               normalized_extension,
                                                               has_extension);
        if (!candidate.has_value()) {
            continue;
        }
        if (is_duplicate_category(results, *candidate)) {
            continue;
        }

        results.push_back(*candidate);
        if (results.size() >= limit) {
            break;
        }
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
