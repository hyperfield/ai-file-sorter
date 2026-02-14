#include "constants.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstdlib>
#include <string>
#include <filesystem>
#include <chrono>


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
    const char* appdata = std::getenv("APPDATA");
    if (appdata && *appdata) {
        return std::string(appdata) + "\\" + APP_NAME_DIR + "\\logs";
    }
    throw std::runtime_error("Failed to determine APPDATA environment variable.");
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
