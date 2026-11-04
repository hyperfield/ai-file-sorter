#include "constants.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstdlib>
#include <string>
#include <filesystem>
#include <chrono>
#include <fstream>

namespace {

bool is_active_log_filename(const std::filesystem::path& path)
{
    const std::string filename = Utils::path_to_utf8(path.filename());
    return filename == "core.log" || filename == "db.log" || filename == "ui.log";
}

bool truncate_log_file(const std::filesystem::path& path, std::string* error)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (stream) {
        return true;
    }

    if (error) {
        *error = "Failed to truncate log file '" + Utils::path_to_utf8(path) + "'.";
    }
    return false;
}

} // namespace


std::string Logger::get_log_directory()
{
#ifdef _WIN32
    return get_windows_log_directory();
#else
    return get_xdg_cache_home();
#endif
}


std::string Logger::get_xdg_cache_home()
{
    const char* xdg_cache_home = std::getenv("XDG_CACHE_HOME");
    if (xdg_cache_home && *xdg_cache_home) {
        return std::string(xdg_cache_home) + "/" + APP_NAME_DIR + "/my_app/logs";
    }

    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.cache/" + APP_NAME_DIR + "/logs";
    }
    throw std::runtime_error("Failed to determine XDG_CACHE_HOME or HOME environment variable.");
}


std::string Logger::get_windows_log_directory() {
    if (const auto appdata = Utils::getenv_utf8("APPDATA")) {
        return *appdata + "\\" + APP_NAME_DIR + "\\logs";
    }
    throw std::runtime_error("Failed to determine APPDATA environment variable.");
}

bool Logger::clear_logs(std::string* error)
{
    std::error_code ec;
    const std::filesystem::path log_dir = Utils::utf8_to_path(get_log_directory());
    if (!std::filesystem::exists(log_dir, ec)) {
        return true;
    }

    if (ec) {
        if (error) {
            *error = "Failed to inspect log directory '" + Utils::path_to_utf8(log_dir) + "'.";
        }
        return false;
    }

    for (const char* name : {"core_logger", "db_logger", "ui_logger"}) {
        if (auto logger = get_logger(name)) {
            logger->flush();
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(log_dir, ec)) {
        if (ec) {
            if (error) {
                *error = "Failed to enumerate log directory '" + Utils::path_to_utf8(log_dir) + "'.";
            }
            return false;
        }

        const std::filesystem::path path = entry.path();
        if (entry.is_directory(ec)) {
            std::filesystem::remove_all(path, ec);
            if (ec) {
                if (error) {
                    *error = "Failed to remove log directory '" + Utils::path_to_utf8(path) + "'.";
                }
                return false;
            }
            continue;
        }

        if (entry.is_regular_file(ec)) {
            if (is_active_log_filename(path)) {
                if (!truncate_log_file(path, error)) {
                    return false;
                }
            } else {
                std::filesystem::remove(path, ec);
                if (ec) {
                    if (error) {
                        *error = "Failed to remove log file '" + Utils::path_to_utf8(path) + "'.";
                    }
                    return false;
                }
            }
            continue;
        }

        std::filesystem::remove(path, ec);
        if (ec) {
            if (error) {
                *error = "Failed to remove log entry '" + Utils::path_to_utf8(path) + "'.";
            }
            return false;
        }
    }

    return true;
}


void Logger::setup_loggers()
{
    std::string log_dir = get_log_directory();
    Utils::ensure_directory_exists(log_dir);

    auto core_log_path = log_dir + "/core.log";
    auto db_log_path = log_dir + "/db.log";
    auto ui_log_path = log_dir + "/ui.log";
    
    auto core_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto core_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(core_log_path, 1048576 * 5, 3);

    auto db_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto db_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(db_log_path, 1048576 * 5, 3);

    auto ui_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto ui_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(ui_log_path, 1048576 * 5, 3);

    auto core_logger = std::make_shared<spdlog::logger>("core_logger", spdlog::sinks_init_list{core_console_sink, core_file_sink});
    auto db_logger = std::make_shared<spdlog::logger>("db_logger", spdlog::sinks_init_list{db_console_sink, db_file_sink});
    auto ui_logger = std::make_shared<spdlog::logger>("ui_logger", spdlog::sinks_init_list{ui_console_sink, ui_file_sink});

    spdlog::register_logger(core_logger);
    spdlog::register_logger(db_logger);
    spdlog::register_logger(ui_logger);

    core_logger->set_level(spdlog::level::debug);
    db_logger->set_level(spdlog::level::debug);
    ui_logger->set_level(spdlog::level::debug);

    core_logger->flush_on(spdlog::level::info);
    db_logger->flush_on(spdlog::level::info);
    ui_logger->flush_on(spdlog::level::info);

    spdlog::flush_every(std::chrono::seconds(2));
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Loggers initialized.");
}


std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string &name) {
    return spdlog::get(name);
}
