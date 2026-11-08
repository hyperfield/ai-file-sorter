#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <string>

class Logger {
public:
    /** @brief Destination for console diagnostics; rotating file logs are always retained. */
    enum class ConsoleOutput { Stdout, Stderr };

    static std::string get_log_directory();
    /**
     * @brief Clears application log files while keeping the log directory in place.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the log directory was cleared or was already empty.
     */
    static bool clear_logs(std::string* error = nullptr);
    /** @brief Initialize application loggers with the desktop's stdout console output. */
    static void setup_loggers();
    /**
     * @brief Initialize application and default loggers for the selected console stream.
     * @param console_output Use stderr when stdout carries headless JSON status records.
     */
    static void setup_loggers(ConsoleOutput console_output);
    static std::shared_ptr<spdlog::logger> get_logger(const std::string &name);
    static std::string get_log_file_path(const std::string &log_dir, const std::string &log_name);

private:
    static std::string get_xdg_cache_home();
    static std::string get_windows_log_directory();
    Logger() = delete;
};

#endif
