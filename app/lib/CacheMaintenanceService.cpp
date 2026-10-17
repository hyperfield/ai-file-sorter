#include "CacheMaintenanceService.hpp"

#include "Logger.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sqlite3.h>
#include <system_error>

namespace {

constexpr std::array<const char*, 3> kKnownLogFiles{
    "core.log",
    "db.log",
    "ui.log"
};

std::filesystem::path categorization_cache_path_for_config_dir(const std::string& config_dir)
{
    const char* cache_file = std::getenv("CATEGORIZATION_CACHE_FILE");
    const char* file_name = (cache_file && *cache_file) ? cache_file : "categorization_results.db";
    return std::filesystem::path(config_dir) / file_name;
}

std::filesystem::path image_location_cache_path_for_config_dir(const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "image_place_cache.db";
}

bool is_known_log_filename(const std::filesystem::path& path)
{
    const auto filename = path.filename().string();
    for (const char* known : kKnownLogFiles) {
        if (filename == known) {
            return true;
        }
    }
    return false;
}

const char* sqlite_cache_table_name(CacheMaintenanceTarget target)
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return "file_categorization";
    case CacheMaintenanceTarget::ImageLocation:
        return "reverse_geocode_cache";
    case CacheMaintenanceTarget::Logs:
        return nullptr;
    }
    return nullptr;
}

bool has_content_evidence(sqlite3* db)
{
    sqlite3_stmt* statement = nullptr;
    const bool prepared = sqlite3_prepare_v2(
        db, "SELECT 1 FROM content_analysis_cache LIMIT 1;", -1, &statement, nullptr) == SQLITE_OK;
    const bool present = prepared && sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return present;
}

} // namespace

CacheMaintenanceService::CacheMaintenanceService(
    std::string config_dir,
    Callbacks callbacks,
    std::function<std::filesystem::path()> log_dir_provider)
    : config_dir_(std::move(config_dir)),
      callbacks_(std::move(callbacks)),
      log_dir_provider_(std::move(log_dir_provider))
{
    if (!log_dir_provider_) {
        log_dir_provider_ = []() {
            return std::filesystem::path(Logger::get_log_directory());
        };
    }
}

CacheMaintenanceTargetInfo CacheMaintenanceService::target_info(CacheMaintenanceTarget target) const
{
    const std::filesystem::path path = target_path(target);
    std::error_code ec;
    const bool exists = !path.empty() && std::filesystem::exists(path, ec) && !ec;
    std::uintmax_t size_bytes = 0;
    if (exists) {
        if (target == CacheMaintenanceTarget::Categorization ||
            target == CacheMaintenanceTarget::ImageLocation) {
            size_bytes = compute_sqlite_cache_size(target, path);
        } else {
            size_bytes = compute_size_bytes(path);
        }
    }
    return CacheMaintenanceTargetInfo{
        .target = target,
        .path = path,
        .size_bytes = size_bytes,
        .exists = exists
    };
}

bool CacheMaintenanceService::clear(CacheMaintenanceTarget target, std::string* error) const
{
    std::string ignored_error;
    std::string& error_ref = error ? *error : ignored_error;

    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        if (callbacks_.clear_categorization_cache) {
            return callbacks_.clear_categorization_cache(error_ref);
        }
        return remove_file_cache(target_path(target), error);
    case CacheMaintenanceTarget::ImageLocation:
        return remove_file_cache(target_path(target), error);
    case CacheMaintenanceTarget::Logs:
        if (callbacks_.clear_logs) {
            return callbacks_.clear_logs(error_ref);
        }
        return clear_directory_cache(target_path(target), error);
    }

    if (error) {
        *error = "Unknown cache target.";
    }
    return false;
}

std::string CacheMaintenanceService::format_size(std::uintmax_t size_bytes)
{
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(size_bytes);
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < std::size(kUnits)) {
        value /= 1024.0;
        ++unit_index;
    }

    char buffer[32];
    if (unit_index == 0) {
        std::snprintf(buffer, sizeof(buffer), "%.0f %s", value, kUnits[unit_index]);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, kUnits[unit_index]);
    }
    return std::string(buffer);
}

std::filesystem::path CacheMaintenanceService::target_path(CacheMaintenanceTarget target) const
{
    try {
        switch (target) {
        case CacheMaintenanceTarget::Categorization:
            return categorization_cache_path_for_config_dir(config_dir_);
        case CacheMaintenanceTarget::ImageLocation:
            return image_location_cache_path_for_config_dir(config_dir_);
        case CacheMaintenanceTarget::Logs:
            return log_dir_provider_ ? log_dir_provider_() : std::filesystem::path();
        }
    } catch (...) {
        return {};
    }
    return {};
}

std::uintmax_t CacheMaintenanceService::compute_size_bytes(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return 0;
    }
    if (std::filesystem::is_regular_file(path, ec)) {
        return ec ? 0 : std::filesystem::file_size(path, ec);
    }

    std::uintmax_t total = 0;
    for (std::filesystem::recursive_directory_iterator it(path, ec), end; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        if (it->is_regular_file(ec) && !ec) {
            total += it->file_size(ec);
            if (ec) {
                ec.clear();
            }
        }
    }
    return total;
}

std::uintmax_t CacheMaintenanceService::compute_sqlite_cache_size(
    CacheMaintenanceTarget target,
    const std::filesystem::path& path)
{
    const std::uintmax_t file_size = compute_size_bytes(path);
    if (file_size == 0) {
        return 0;
    }

    const char* table_name = sqlite_cache_table_name(target);
    if (!table_name) {
        return file_size;
    }

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) {
            sqlite3_close(db);
        }
        return file_size;
    }

    std::uintmax_t estimated_size = file_size;
    const std::string sql = "SELECT COUNT(*) FROM " + std::string(table_name) + ";";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int64(stmt, 0) == 0) {
            estimated_size = 0;
        }
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (target == CacheMaintenanceTarget::Categorization && has_content_evidence(db)) {
        estimated_size = file_size;
    }
    sqlite3_close(db);
    return estimated_size;
}

bool CacheMaintenanceService::remove_file_cache(const std::filesystem::path& path, std::string* error)
{
    if (path.empty()) {
        if (error) {
            *error = "Cache path is unavailable.";
        }
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return !ec;
    }
    if (ec) {
        if (error) {
            *error = "Failed to inspect cache path '" + path.string() + "'.";
        }
        return false;
    }

    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) {
        if (error) {
            *error = "Failed to remove cache file '" + path.string() + "'.";
        }
        return false;
    }
    return true;
}

bool CacheMaintenanceService::clear_directory_cache(const std::filesystem::path& path, std::string* error)
{
    if (path.empty()) {
        if (error) {
            *error = "Cache path is unavailable.";
        }
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return !ec;
    }
    if (ec) {
        if (error) {
            *error = "Failed to inspect cache path '" + path.string() + "'.";
        }
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) {
            if (error) {
                *error = "Failed to enumerate cache directory '" + path.string() + "'.";
            }
            return false;
        }

        const std::filesystem::path entry_path = entry.path();
        if (entry.is_directory(ec)) {
            std::filesystem::remove_all(entry_path, ec);
        } else if (!is_known_log_filename(entry_path)) {
            std::filesystem::remove(entry_path, ec);
        } else {
            std::ofstream trunc(entry_path, std::ios::binary | std::ios::trunc);
            if (!trunc) {
                ec = std::make_error_code(std::errc::io_error);
            }
        }

        if (ec) {
            if (error) {
                *error = "Failed to clear cache entry '" + entry_path.string() + "'.";
            }
            return false;
        }
    }

    return true;
}
