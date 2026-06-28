#include "DatabaseManager.hpp"
#include "Types.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace {
constexpr double kSimilarityThreshold = 0.85;
constexpr char kLegacyMusicCategoryNormalized[] = "music";
constexpr char kCanonicalAudioCategoryNormalized[] = "audio";
constexpr char kCanonicalAudioCategoryDisplay[] = "Audio";
constexpr char kLegacyInstallerBuildersNormalized[] = "installer builders";
constexpr char kCanonicalInstallerToolsNormalized[] = "installer tools";
constexpr char kCanonicalInstallerToolsDisplay[] = "Installer Tools";
constexpr char kArchivesCategoryNormalized[] = "archives";
constexpr char kArchivesCategoryDisplay[] = "Archives";
constexpr char kDataExportsCategoryNormalized[] = "data exports";
constexpr char kDataExportsCategoryDisplay[] = "Data Exports";
constexpr char kDocumentsCategoryNormalized[] = "documents";
constexpr char kDocumentsCategoryDisplay[] = "Documents";
constexpr char kDriversCategoryNormalized[] = "drivers";
constexpr char kDriversCategoryDisplay[] = "Drivers";
constexpr char kFontsCategoryNormalized[] = "fonts";
constexpr char kFontsCategoryDisplay[] = "Fonts";
constexpr char kImagesCategoryNormalized[] = "images";
constexpr char kImagesCategoryDisplay[] = "Images";
constexpr char kInstallersCategoryNormalized[] = "installers";
constexpr char kInstallersCategoryDisplay[] = "Installers";
constexpr char kOperatingSystemsCategoryNormalized[] = "operating systems";
constexpr char kOperatingSystemsCategoryDisplay[] = "Operating Systems";
constexpr char kPresentationsCategoryNormalized[] = "presentations";
constexpr char kPresentationsCategoryDisplay[] = "Presentations";
constexpr char kSoftwareCategoryNormalized[] = "software";
constexpr char kSoftwareCategoryDisplay[] = "Software";
constexpr char kSpreadsheetsCategoryNormalized[] = "spreadsheets";
constexpr char kSpreadsheetsCategoryDisplay[] = "Spreadsheets";
constexpr char kVideosCategoryNormalized[] = "videos";
constexpr char kVideosCategoryDisplay[] = "Videos";

bool apply_legacy_display_label_migrations(const std::string& normalized_category,
                                           const std::string& normalized_subcategory,
                                           std::string& category,
                                           std::string& subcategory) {
    bool changed = false;

    if (normalized_category == kLegacyMusicCategoryNormalized) {
        category = kCanonicalAudioCategoryDisplay;
        changed = true;
    }

    if (normalized_subcategory == kLegacyInstallerBuildersNormalized) {
        subcategory = kCanonicalInstallerToolsDisplay;
        changed = true;
    }

    return changed;
}

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

std::string language_storage_key(CategoryLanguage language) {
    return to_lower_copy(categoryLanguageDisplay(language));
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

std::string strip_inline_translation_label_artifacts(std::string value, bool category_label)
{
    const std::vector<std::string_view> labels = category_label
        ? std::vector<std::string_view>{"subcategory", "sub category", "sub_category"}
        : std::vector<std::string_view>{"category", "main category", "main_category"};
    const std::string lower = to_lower_copy(value);

    for (std::size_t idx = 0; idx < lower.size(); ++idx) {
        const char delimiter = lower[idx];
        if (delimiter != ',' && delimiter != ';' && delimiter != '-') {
            continue;
        }

        std::size_t label_start = idx + 1;
        while (label_start < lower.size() &&
               std::isspace(static_cast<unsigned char>(lower[label_start]))) {
            ++label_start;
        }

        for (const std::string_view label : labels) {
            if (label_start + label.size() > lower.size()) {
                continue;
            }
            if (lower.compare(label_start, label.size(), label) != 0) {
                continue;
            }

            const std::size_t after_label = label_start + label.size();
            if (after_label == lower.size() ||
                std::isspace(static_cast<unsigned char>(lower[after_label])) ||
                lower[after_label] == ':' ||
                lower[after_label] == '=') {
                value.resize(idx);
                return trim_copy(std::move(value));
            }
        }
    }

    return trim_copy(std::move(value));
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
    std::string cat = Utils::sanitize_path_label(category ? category : "");
    std::string subcat = Utils::sanitize_path_label(subcategory ? subcategory : "");
    std::string suggested = Utils::sanitize_path_label(suggested_name ? suggested_name : "");

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
    entry.canonical_category = cat;
    entry.canonical_subcategory = subcat;
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
    if (migrate_legacy_taxonomy_labels()) {
        load_taxonomy_cache();
    }
    load_translation_cache();
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

    const char* translation_sql = R"(
        CREATE TABLE IF NOT EXISTS category_translation (
            taxonomy_id INTEGER NOT NULL,
            language TEXT NOT NULL,
            category TEXT NOT NULL,
            subcategory TEXT NOT NULL,
            normalized_category TEXT NOT NULL,
            normalized_subcategory TEXT NOT NULL,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(taxonomy_id, language),
            FOREIGN KEY(taxonomy_id) REFERENCES category_taxonomy(id)
        );
    )";
    if (sqlite3_exec(db, translation_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create category_translation table: {}", error_msg);
        sqlite3_free(error_msg);
    }

    const char* translation_lookup_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_category_translation_lookup "
        "ON category_translation(language, normalized_category, normalized_subcategory);";
    if (sqlite3_exec(db, translation_lookup_index_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to create translation lookup index: {}", error_msg);
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

void DatabaseManager::load_translation_cache() {
    translation_entries.clear();
    translation_lookup.clear();

    if (!db) {
        return;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* select_translation =
        "SELECT taxonomy_id, language, category, subcategory, normalized_category, normalized_subcategory "
        "FROM category_translation;";

    if (sqlite3_prepare_v2(db, select_translation, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const int taxonomy_id = sqlite3_column_int(stmt, 0);
            const char* language_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* category_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* subcategory_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* normalized_category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* normalized_subcategory = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

            CategoryLanguage language = categoryLanguageFromString(QString::fromStdString(language_text ? language_text : ""));
            ResolvedCategory translated{
                taxonomy_id,
                category_text ? category_text : "",
                subcategory_text ? subcategory_text : ""
            };
            translation_entries[make_translation_entry_key(taxonomy_id, language)] = translated;
            translation_lookup[make_translation_lookup_key(language,
                                                           normalized_category ? normalized_category : "",
                                                           normalized_subcategory ? normalized_subcategory : "")] = taxonomy_id;
        }
    } else {
        db_log(spdlog::level::err, "Failed to load category translations: {}", sqlite3_errmsg(db));
    }
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

bool DatabaseManager::migrate_legacy_taxonomy_labels() {
    if (!db) {
        return false;
    }

    struct LegacyTaxonomyRow {
        int id;
        std::string category;
        std::string subcategory;
        std::string normalized_category;
        std::string normalized_subcategory;
    };

    struct LegacyFileRow {
        int id;
        std::string category;
        std::string subcategory;
    };

    const auto update_file_row = [&](int row_id,
                                     const std::string& category,
                                     const std::string& subcategory,
                                     int taxonomy_id) -> bool {
        static constexpr char kUpdateFileSql[] =
            "UPDATE file_categorization SET category = ?, subcategory = ?, taxonomy_id = ? WHERE id = ?;";
        StatementPtr update_stmt = prepare_statement(db, kUpdateFileSql);
        if (!update_stmt) {
            db_log(spdlog::level::err, "Failed to prepare file migration update: {}", sqlite3_errmsg(db));
            return false;
        }

        sqlite3_bind_text(update_stmt.get(), 1, category.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(update_stmt.get(), 2, subcategory.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(update_stmt.get(), 3, taxonomy_id);
        sqlite3_bind_int(update_stmt.get(), 4, row_id);
        if (sqlite3_step(update_stmt.get()) != SQLITE_DONE) {
            db_log(spdlog::level::err, "Failed to update migrated file row {}: {}", row_id, sqlite3_errmsg(db));
            return false;
        }

        return true;
    };

    const auto retarget_legacy_taxonomy = [&](const LegacyTaxonomyRow& legacy_row,
                                              const ResolvedCategory& target) -> bool {
        ensure_alias_mapping(target.taxonomy_id,
                             legacy_row.normalized_category,
                             legacy_row.normalized_subcategory);

        static constexpr char kMoveAliasesSql[] =
            "UPDATE OR IGNORE category_alias SET taxonomy_id = ? WHERE taxonomy_id = ?;";
        StatementPtr move_aliases_stmt = prepare_statement(db, kMoveAliasesSql);
        if (!move_aliases_stmt) {
            db_log(spdlog::level::err, "Failed to prepare alias migration: {}", sqlite3_errmsg(db));
            return false;
        }
        sqlite3_bind_int(move_aliases_stmt.get(), 1, target.taxonomy_id);
        sqlite3_bind_int(move_aliases_stmt.get(), 2, legacy_row.id);
        if (sqlite3_step(move_aliases_stmt.get()) != SQLITE_DONE) {
            db_log(spdlog::level::err, "Failed to migrate aliases for taxonomy {}: {}", legacy_row.id, sqlite3_errmsg(db));
            return false;
        }

        static constexpr char kDeleteLegacyTranslationsSql[] =
            "DELETE FROM category_translation WHERE taxonomy_id = ?;";
        StatementPtr delete_translations_stmt = prepare_statement(db, kDeleteLegacyTranslationsSql);
        if (!delete_translations_stmt) {
            db_log(spdlog::level::err, "Failed to prepare translation cleanup: {}", sqlite3_errmsg(db));
            return false;
        }
        sqlite3_bind_int(delete_translations_stmt.get(), 1, legacy_row.id);
        if (sqlite3_step(delete_translations_stmt.get()) != SQLITE_DONE) {
            db_log(spdlog::level::err, "Failed to delete legacy translations for taxonomy {}: {}", legacy_row.id, sqlite3_errmsg(db));
            return false;
        }

        static constexpr char kRetargetTaxonomyReferencesSql[] =
            "UPDATE file_categorization SET taxonomy_id = ? WHERE taxonomy_id = ?;";
        StatementPtr retarget_files_stmt = prepare_statement(db, kRetargetTaxonomyReferencesSql);
        if (!retarget_files_stmt) {
            db_log(spdlog::level::err, "Failed to prepare taxonomy reference migration: {}", sqlite3_errmsg(db));
            return false;
        }
        sqlite3_bind_int(retarget_files_stmt.get(), 1, target.taxonomy_id);
        sqlite3_bind_int(retarget_files_stmt.get(), 2, legacy_row.id);
        if (sqlite3_step(retarget_files_stmt.get()) != SQLITE_DONE) {
            db_log(spdlog::level::err, "Failed to retarget cached rows from taxonomy {}: {}", legacy_row.id, sqlite3_errmsg(db));
            return false;
        }

        static constexpr char kDeleteLegacyTaxonomySql[] =
            "DELETE FROM category_taxonomy WHERE id = ?;";
        StatementPtr delete_taxonomy_stmt = prepare_statement(db, kDeleteLegacyTaxonomySql);
        if (!delete_taxonomy_stmt) {
            db_log(spdlog::level::err, "Failed to prepare taxonomy cleanup: {}", sqlite3_errmsg(db));
            return false;
        }
        sqlite3_bind_int(delete_taxonomy_stmt.get(), 1, legacy_row.id);
        if (sqlite3_step(delete_taxonomy_stmt.get()) != SQLITE_DONE) {
            db_log(spdlog::level::err, "Failed to delete legacy taxonomy row {}: {}", legacy_row.id, sqlite3_errmsg(db));
            return false;
        }

        return true;
    };

    std::vector<LegacyTaxonomyRow> legacy_taxonomy_rows;
    static constexpr char kSelectLegacyTaxonomySql[] =
        "SELECT id, canonical_category, canonical_subcategory, normalized_category, normalized_subcategory "
        "FROM category_taxonomy "
        "WHERE normalized_category = ? OR normalized_subcategory = ?;";
    StatementPtr taxonomy_stmt = prepare_statement(db, kSelectLegacyTaxonomySql);
    if (!taxonomy_stmt) {
        db_log(spdlog::level::err, "Failed to prepare legacy taxonomy scan: {}", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(taxonomy_stmt.get(), 1, kLegacyMusicCategoryNormalized, -1, SQLITE_STATIC);
    sqlite3_bind_text(taxonomy_stmt.get(), 2, kLegacyInstallerBuildersNormalized, -1, SQLITE_STATIC);
    while (sqlite3_step(taxonomy_stmt.get()) == SQLITE_ROW) {
        const char* category_text = reinterpret_cast<const char*>(sqlite3_column_text(taxonomy_stmt.get(), 1));
        const char* subcategory_text = reinterpret_cast<const char*>(sqlite3_column_text(taxonomy_stmt.get(), 2));
        const char* normalized_category_text = reinterpret_cast<const char*>(sqlite3_column_text(taxonomy_stmt.get(), 3));
        const char* normalized_subcategory_text = reinterpret_cast<const char*>(sqlite3_column_text(taxonomy_stmt.get(), 4));
        legacy_taxonomy_rows.push_back({
            sqlite3_column_int(taxonomy_stmt.get(), 0),
            category_text ? category_text : "",
            subcategory_text ? subcategory_text : "",
            normalized_category_text ? normalized_category_text : "",
            normalized_subcategory_text ? normalized_subcategory_text : "",
        });
    }

    std::vector<LegacyFileRow> legacy_file_rows;
    static constexpr char kSelectLegacyFilesSql[] =
        "SELECT id, category, subcategory "
        "FROM file_categorization "
        "WHERE lower(trim(category)) = ? OR lower(trim(subcategory)) = ?;";
    StatementPtr file_stmt = prepare_statement(db, kSelectLegacyFilesSql);
    if (!file_stmt) {
        db_log(spdlog::level::err, "Failed to prepare legacy file scan: {}", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(file_stmt.get(), 1, kLegacyMusicCategoryNormalized, -1, SQLITE_STATIC);
    sqlite3_bind_text(file_stmt.get(), 2, kLegacyInstallerBuildersNormalized, -1, SQLITE_STATIC);
    while (sqlite3_step(file_stmt.get()) == SQLITE_ROW) {
        const char* category_text = reinterpret_cast<const char*>(sqlite3_column_text(file_stmt.get(), 1));
        const char* subcategory_text = reinterpret_cast<const char*>(sqlite3_column_text(file_stmt.get(), 2));
        legacy_file_rows.push_back({
            sqlite3_column_int(file_stmt.get(), 0),
            category_text ? category_text : "",
            subcategory_text ? subcategory_text : "",
        });
    }

    if (legacy_taxonomy_rows.empty() && legacy_file_rows.empty()) {
        return false;
    }

    taxonomy_stmt.reset();
    file_stmt.reset();

    char* error_msg = nullptr;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to begin legacy taxonomy migration: {}", error_msg ? error_msg : sqlite3_errmsg(db));
        sqlite3_free(error_msg);
        return false;
    }

    bool changed = false;
    bool success = true;

    for (const auto& legacy_row : legacy_taxonomy_rows) {
        std::string target_category = legacy_row.category;
        std::string target_subcategory = legacy_row.subcategory;
        if (!apply_legacy_display_label_migrations(legacy_row.normalized_category,
                                                   legacy_row.normalized_subcategory,
                                                   target_category,
                                                   target_subcategory)) {
            continue;
        }

        const ResolvedCategory target = resolve_category(target_category, target_subcategory);
        if (target.taxonomy_id <= 0 || !retarget_legacy_taxonomy(legacy_row, target)) {
            success = false;
            break;
        }
        changed = true;
    }

    for (const auto& legacy_row : legacy_file_rows) {
        if (!success) {
            break;
        }

        const std::string normalized_category = normalize_label(legacy_row.category);
        const std::string normalized_subcategory = normalize_label(legacy_row.subcategory);
        std::string target_category = legacy_row.category;
        std::string target_subcategory = legacy_row.subcategory;
        if (!apply_legacy_display_label_migrations(normalized_category,
                                                   normalized_subcategory,
                                                   target_category,
                                                   target_subcategory)) {
            continue;
        }

        const ResolvedCategory target = resolve_category(target_category, target_subcategory);
        if (target.taxonomy_id <= 0 ||
            !update_file_row(legacy_row.id, target.category, target.subcategory, target.taxonomy_id)) {
            success = false;
            break;
        }
        changed = true;
    }

    if (success) {
        refresh_taxonomy_frequencies();
    }

    const char* finish_sql = success ? "COMMIT;" : "ROLLBACK;";
    if (sqlite3_exec(db, finish_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to finish legacy taxonomy migration: {}", error_msg ? error_msg : sqlite3_errmsg(db));
        sqlite3_free(error_msg);
        return false;
    }
    if (error_msg) {
        sqlite3_free(error_msg);
    }

    if (!success) {
        return false;
    }

    if (changed) {
        db_log(spdlog::level::info, "Migrated legacy taxonomy labels to canonical Audio/Installer Tools labels");
    }
    return changed;
}

void DatabaseManager::refresh_taxonomy_frequencies() {
    if (!db) {
        return;
    }

    static constexpr char kRefreshFrequenciesSql[] =
        "UPDATE category_taxonomy "
        "SET frequency = (SELECT COUNT(*) FROM file_categorization WHERE taxonomy_id = category_taxonomy.id);";
    char* error_msg = nullptr;
    if (sqlite3_exec(db, kRefreshFrequenciesSql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to refresh taxonomy frequencies: {}", error_msg ? error_msg : sqlite3_errmsg(db));
        sqlite3_free(error_msg);
    }
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

std::string DatabaseManager::make_translation_entry_key(int taxonomy_id,
                                                        CategoryLanguage language) {
    return fmt::format("{}|{}", taxonomy_id, language_storage_key(language));
}

std::string DatabaseManager::make_translation_lookup_key(CategoryLanguage language,
                                                         const std::string& norm_category,
                                                         const std::string& norm_subcategory) {
    return fmt::format("{}|{}|{}",
                       language_storage_key(language),
                       norm_category,
                       norm_subcategory);
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

std::optional<CanonicalCategoryLabel> canonicalize_broad_main_label(const std::string& normalized_label)
{
    static const std::unordered_map<std::string, CanonicalCategoryLabel> kBroadMainLabels = {
        {"archive", {kArchivesCategoryNormalized, kArchivesCategoryDisplay}},
        {"archives", {kArchivesCategoryNormalized, kArchivesCategoryDisplay}},
        {"audio", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio file", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio files", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"config", {"configs", "Configs"}},
        {"configs", {"configs", "Configs"}},
        {"configuration", {"configs", "Configs"}},
        {"configurations", {"configs", "Configs"}},
        {"data export", {kDataExportsCategoryNormalized, kDataExportsCategoryDisplay}},
        {"data exports", {kDataExportsCategoryNormalized, kDataExportsCategoryDisplay}},
        {"document", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"documents", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"doc", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"docs", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"driver", {kDriversCategoryNormalized, kDriversCategoryDisplay}},
        {"drivers", {kDriversCategoryNormalized, kDriversCategoryDisplay}},
        {"ebook", {"ebooks", "Ebooks"}},
        {"ebooks", {"ebooks", "Ebooks"}},
        {"font", {kFontsCategoryNormalized, kFontsCategoryDisplay}},
        {"fonts", {kFontsCategoryNormalized, kFontsCategoryDisplay}},
        {"image", {kImagesCategoryNormalized, kImagesCategoryDisplay}},
        {"images", {kImagesCategoryNormalized, kImagesCategoryDisplay}},
        {"installer", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"installers", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"installation", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"installations", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"operating system", {kOperatingSystemsCategoryNormalized, kOperatingSystemsCategoryDisplay}},
        {"operating systems", {kOperatingSystemsCategoryNormalized, kOperatingSystemsCategoryDisplay}},
        {"presentation", {kPresentationsCategoryNormalized, kPresentationsCategoryDisplay}},
        {"presentations", {kPresentationsCategoryNormalized, kPresentationsCategoryDisplay}},
        {"software", {kSoftwareCategoryNormalized, kSoftwareCategoryDisplay}},
        {"spreadsheet", {kSpreadsheetsCategoryNormalized, kSpreadsheetsCategoryDisplay}},
        {"spreadsheets", {kSpreadsheetsCategoryNormalized, kSpreadsheetsCategoryDisplay}},
        {"video", {kVideosCategoryNormalized, kVideosCategoryDisplay}},
        {"videos", {kVideosCategoryNormalized, kVideosCategoryDisplay}}
    };

    if (auto it = kBroadMainLabels.find(normalized_label); it != kBroadMainLabels.end()) {
        return it->second;
    }

    const std::string stripped_label = strip_trailing_stopwords(normalized_label);
    if (auto it = kBroadMainLabels.find(stripped_label); it != kBroadMainLabels.end()) {
        return it->second;
    }

    return std::nullopt;
}

struct SemanticFamily {
    std::string canonical_category_normalized;
    std::string canonical_category_display;
    std::string parent_category_normalized;
    std::vector<std::string_view> aliases;
    std::vector<std::string_view> parent_generic_aliases;
};

bool is_generic_family_subcategory(const std::string& normalized_subcategory,
                                   const std::string& normalized_category,
                                   const SemanticFamily& family) {
    if (normalized_subcategory.empty()) {
        return true;
    }

    static const std::unordered_set<std::string> kGeneric = {
        "general",
        "misc",
        "miscellaneous",
        "other",
        "others",
        "uncategorized",
        "document",
        "documents",
        "doc",
        "docs"
    };

    if (kGeneric.contains(normalized_subcategory)) {
        return true;
    }

    const std::string stripped_subcategory = strip_trailing_stopwords(normalized_subcategory);
    if (kGeneric.contains(stripped_subcategory)) {
        return true;
    }

    if (std::any_of(family.aliases.begin(),
                    family.aliases.end(),
                    [&](std::string_view alias) {
                        return normalized_subcategory == alias || stripped_subcategory == alias;
                    })) {
        return true;
    }

    if (stripped_subcategory == strip_trailing_stopwords(normalized_category) ||
        stripped_subcategory == family.canonical_category_normalized ||
        stripped_subcategory == strip_trailing_stopwords(family.parent_category_normalized)) {
        return true;
    }

    return std::any_of(family.parent_generic_aliases.begin(),
                       family.parent_generic_aliases.end(),
                       [&](std::string_view alias) { return stripped_subcategory == alias; });
}

const std::vector<SemanticFamily>& semantic_families() {
    static const std::vector<SemanticFamily> kFamilies = {
        {
            "backups", "Backups", "archives",
            {"backup", "backups", "backup file", "backup files"},
            {"archive", "archives"}
        },
        {
            "ebooks", "Ebooks", "books",
            {"ebook", "ebooks", "e book", "e books"},
            {"book", "books"}
        },
        {
            "guides", "Guides", "documents",
            {"guide", "guides"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "licenses", "Licenses", "documents",
            {"license", "licenses", "licence", "licences"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "manuals", "Manuals", "documents",
            {"manual", "manuals"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "presentations", "Presentations", "documents",
            {"presentation", "presentations", "slide deck", "slide decks", "deck", "decks", "slides"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "spreadsheets", "Spreadsheets", "documents",
            {"spreadsheet", "spreadsheets", "worksheet", "worksheets"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files", "table", "tables"}
        }
    };
    return kFamilies;
}

bool semantic_family_matches_alias(const SemanticFamily& family, const std::string& normalized_label) {
    if (normalized_label.empty()) {
        return false;
    }

    const std::string stripped = strip_trailing_stopwords(normalized_label);
    return std::any_of(family.aliases.begin(),
                       family.aliases.end(),
                       [&](std::string_view alias) {
                           return normalized_label == alias || stripped == alias;
                       });
}

const SemanticFamily* find_semantic_family(const std::string& normalized_label) {
    for (const auto& family : semantic_families()) {
        if (semantic_family_matches_alias(family, normalized_label)) {
            return &family;
        }
    }
    return nullptr;
}

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
    if (const auto broad_main = canonicalize_broad_main_label(normalized_category)) {
        return *broad_main;
    }

    static const std::unordered_map<std::string, CanonicalCategoryLabel> kCategorySynonyms = {
        {"audio", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio file", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio files", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"music", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},

        {"document", {"documents", "Documents"}},
        {"documents", {"documents", "Documents"}},
        {"doc", {"documents", "Documents"}},
        {"docs", {"documents", "Documents"}},
        {"text", {"documents", "Documents"}},
        {"texts", {"documents", "Documents"}},
        {"paper", {"documents", "Documents"}},
        {"papers", {"documents", "Documents"}},
        {"table", {"documents", "Documents"}},
        {"tables", {"documents", "Documents"}},
        {"office file", {"documents", "Documents"}},
        {"office files", {"documents", "Documents"}},

        {"application", {"software", "Software"}},
        {"applications", {"software", "Software"}},
        {"app", {"software", "Software"}},
        {"apps", {"software", "Software"}},
        {"program", {"software", "Software"}},
        {"programs", {"software", "Software"}},
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

    if (norm_subcategory == kLegacyInstallerBuildersNormalized) {
        trimmed_subcategory = kCanonicalInstallerToolsDisplay;
        norm_subcategory = kCanonicalInstallerToolsNormalized;
    }

    if (const auto canonical_subcategory = canonicalize_broad_main_label(norm_subcategory);
        canonical_subcategory.has_value() &&
        canonical_subcategory->normalized == norm_category) {
        trimmed_subcategory = "General";
        norm_subcategory = normalize_label(trimmed_subcategory);
    }

    if (const SemanticFamily* family_from_category = find_semantic_family(norm_category)) {
        trimmed_category = family_from_category->canonical_category_display;
        norm_category = family_from_category->canonical_category_normalized;
        if (is_generic_family_subcategory(norm_subcategory, norm_category, *family_from_category)) {
            trimmed_subcategory = "General";
            norm_subcategory = normalize_label(trimmed_subcategory);
        }
    } else if (const SemanticFamily* family_from_subcategory = find_semantic_family(norm_subcategory);
               family_from_subcategory &&
               family_from_subcategory->parent_category_normalized == norm_category) {
        trimmed_category = family_from_subcategory->canonical_category_display;
        norm_category = family_from_subcategory->canonical_category_normalized;
        trimmed_subcategory = "General";
        norm_subcategory = normalize_label(trimmed_subcategory);
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

DatabaseManager::ResolvedCategory
DatabaseManager::resolve_category_for_language(const std::string& category,
                                               const std::string& subcategory,
                                               CategoryLanguage language) {
    if (language == CategoryLanguage::English || !db) {
        return resolve_category(category, subcategory);
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
        return resolve_category(category, subcategory);
    }
    if (trimmed_subcategory.empty()) {
        trimmed_subcategory = "General";
    }

    const std::string normalized_category = normalize_label(trimmed_category);
    const std::string normalized_subcategory = normalize_label(trimmed_subcategory);
    const std::string stripped_subcategory = strip_trailing_stopwords(normalized_subcategory);

    const auto lookup_translation = [&](const std::string& norm_subcategory) -> int {
        const auto it = translation_lookup.find(make_translation_lookup_key(language,
                                                                            normalized_category,
                                                                            norm_subcategory));
        return it != translation_lookup.end() ? it->second : -1;
    };

    int taxonomy_id = lookup_translation(stripped_subcategory);
    if (taxonomy_id == -1 && stripped_subcategory != normalized_subcategory) {
        taxonomy_id = lookup_translation(normalized_subcategory);
    }
    if (taxonomy_id != -1) {
        if (const auto* entry = find_taxonomy_entry(taxonomy_id)) {
            return ResolvedCategory{entry->id, entry->category, entry->subcategory};
        }
        return ResolvedCategory{taxonomy_id, trimmed_category, trimmed_subcategory};
    }

    return resolve_category(category, subcategory);
}

std::optional<DatabaseManager::ResolvedCategory>
DatabaseManager::get_category_translation(int taxonomy_id,
                                          CategoryLanguage language) const {
    if (language == CategoryLanguage::English || taxonomy_id <= 0) {
        return std::nullopt;
    }
    const auto it = translation_entries.find(make_translation_entry_key(taxonomy_id, language));
    if (it == translation_entries.end()) {
        return std::nullopt;
    }
    return it->second;
}

DatabaseManager::ResolvedCategory
DatabaseManager::localize_category(const ResolvedCategory& resolved,
                                   CategoryLanguage language) const {
    if (language == CategoryLanguage::English || resolved.taxonomy_id <= 0) {
        return resolved;
    }
    if (const auto translated = get_category_translation(resolved.taxonomy_id, language)) {
        return *translated;
    }
    if (const auto* entry = find_taxonomy_entry(resolved.taxonomy_id)) {
        return ResolvedCategory{entry->id, entry->category, entry->subcategory};
    }
    return resolved;
}

CategorizedFile DatabaseManager::localize_categorized_file(const CategorizedFile& entry,
                                                           CategoryLanguage language) const {
    CategorizedFile localized = entry;
    if (localized.canonical_category.empty()) {
        localized.canonical_category = entry.category;
    }
    if (localized.canonical_subcategory.empty()) {
        localized.canonical_subcategory = entry.subcategory;
    }
    if (const auto translated = get_category_translation(entry.taxonomy_id, language)) {
        localized.category = translated->category;
        localized.subcategory = translated->subcategory;
    }
    return localized;
}

bool DatabaseManager::upsert_category_translation(int taxonomy_id,
                                                  CategoryLanguage language,
                                                  const std::string& category,
                                                  const std::string& subcategory) {
    if (!db || taxonomy_id <= 0 || language == CategoryLanguage::English) {
        return false;
    }

    const std::string sanitized_category = strip_inline_translation_label_artifacts(
        Utils::sanitize_path_label(category),
        true);
    const std::string sanitized_subcategory = strip_inline_translation_label_artifacts(
        Utils::sanitize_path_label(subcategory),
        false);
    if (sanitized_category.empty() || sanitized_subcategory.empty()) {
        return false;
    }

    const std::string normalized_category = normalize_label(sanitized_category);
    const std::string normalized_subcategory = normalize_label(sanitized_subcategory);
    const std::string language_key = language_storage_key(language);

    const char* sql = R"(
        INSERT INTO category_translation
            (taxonomy_id, language, category, subcategory, normalized_category, normalized_subcategory, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
        ON CONFLICT(taxonomy_id, language)
        DO UPDATE SET
            category = excluded.category,
            subcategory = excluded.subcategory,
            normalized_category = excluded.normalized_category,
            normalized_subcategory = excluded.normalized_subcategory,
            updated_at = CURRENT_TIMESTAMP;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare translation upsert: {}", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int(stmt, 1, taxonomy_id);
    sqlite3_bind_text(stmt, 2, language_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, sanitized_category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, sanitized_subcategory.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, normalized_category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, normalized_subcategory.c_str(), -1, SQLITE_TRANSIENT);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        db_log(spdlog::level::err, "Failed to upsert category translation: {}", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    if (!success) {
        return false;
    }

    const ResolvedCategory translated{taxonomy_id, sanitized_category, sanitized_subcategory};
    translation_entries[make_translation_entry_key(taxonomy_id, language)] = translated;
    translation_lookup[make_translation_lookup_key(language,
                                                   normalized_category,
                                                   normalized_subcategory)] = taxonomy_id;
    return true;
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

bool DatabaseManager::clear_directory_categorizations(const std::string& dir_path,
                                                      bool recursive) {
    if (!db) {
        return false;
    }

    const char* sql = recursive
        ? "DELETE FROM file_categorization WHERE dir_path = ? OR dir_path LIKE ? ESCAPE '\\';"
        : "DELETE FROM file_categorization WHERE dir_path = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::err, "Failed to prepare directory cache clear statement: {}", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT);
    if (recursive) {
        const std::string pattern = build_recursive_dir_pattern(dir_path);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        db_log(spdlog::level::err, "Failed to clear cached categorizations for '{}': {}", dir_path, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    cached_results.clear();
    return success;
}

bool DatabaseManager::clear_all_categorizations(bool clear_taxonomy)
{
    if (!db) {
        return false;
    }

    char* error_msg = nullptr;
    const char* delete_sql = clear_taxonomy
        ? "DELETE FROM category_translation;"
          "DELETE FROM category_alias;"
          "DELETE FROM category_taxonomy;"
          "DELETE FROM file_categorization;"
        : "DELETE FROM file_categorization;";
    if (sqlite3_exec(db, delete_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::err,
               clear_taxonomy
                   ? "Failed to clear cached categorizations and taxonomy: {}"
                   : "Failed to clear cached categorizations: {}",
               error_msg ? error_msg : sqlite3_errmsg(db));
        if (error_msg) {
            sqlite3_free(error_msg);
        }
        return false;
    }

    const char* reset_sequence_sql = clear_taxonomy
        ? "DELETE FROM sqlite_sequence WHERE name IN ('file_categorization', 'category_taxonomy');"
        : "DELETE FROM sqlite_sequence WHERE name = 'file_categorization';";
    if (sqlite3_exec(db, reset_sequence_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::warn,
               clear_taxonomy
                   ? "Failed to reset categorization cache/taxonomy sequence: {}"
                   : "Failed to reset categorization cache sequence: {}",
               error_msg ? error_msg : sqlite3_errmsg(db));
        if (error_msg) {
            sqlite3_free(error_msg);
            error_msg = nullptr;
        }
    }

    const char* vacuum_sql = "VACUUM;";
    if (sqlite3_exec(db, vacuum_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        db_log(spdlog::level::warn,
               "Failed to vacuum categorization cache after clear: {}",
               error_msg ? error_msg : sqlite3_errmsg(db));
        if (error_msg) {
            sqlite3_free(error_msg);
        }
    }

    cached_results.clear();
    if (clear_taxonomy) {
        taxonomy_entries.clear();
        canonical_lookup.clear();
        alias_lookup.clear();
        taxonomy_index.clear();
        translation_entries.clear();
        translation_lookup.clear();
    }
    return true;
}

bool DatabaseManager::has_categorization_style_conflict(const std::string& dir_path,
                                                        bool desired_style,
                                                        bool recursive) const {
    if (!db) {
        return false;
    }

    const char* sql = recursive
        ? "SELECT 1 FROM file_categorization "
          "WHERE (dir_path = ? OR dir_path LIKE ? ESCAPE '\\') "
          "AND IFNULL(categorization_style, 0) != ? "
          "LIMIT 1;"
        : "SELECT 1 FROM file_categorization "
          "WHERE dir_path = ? AND IFNULL(categorization_style, 0) != ? "
          "LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_log(spdlog::level::warn, "Failed to prepare cached style conflict query: {}", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, dir_path.c_str(), -1, SQLITE_TRANSIENT);
    int bind_index = 2;
    if (recursive) {
        const std::string pattern = build_recursive_dir_pattern(dir_path);
        sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, bind_index, desired_style ? 1 : 0);

    const bool conflict = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return conflict;
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

std::vector<std::pair<std::string, std::string>> DatabaseManager::get_taxonomy_snapshot(
    std::size_t max_entries,
    CategoryLanguage language) const
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
        ResolvedCategory resolved{entry.id, entry.category, entry.subcategory};
        const ResolvedCategory localized = localize_category(resolved, language);
        snapshot.emplace_back(localized.category, localized.subcategory);
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
