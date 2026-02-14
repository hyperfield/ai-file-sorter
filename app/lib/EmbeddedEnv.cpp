#include "EmbeddedEnv.hpp"
#include "Logger.hpp"

#include <QFile>
#include <QIODevice>
#include <QByteArray>
#include <QString>

#include <sstream>
#include <cstdlib>
#include <stdexcept>


EmbeddedEnv::EmbeddedEnv(const std::string& resource_path)
    : resource_path_(resource_path)
{
}


void EmbeddedEnv::load_env() {
    auto logger = Logger::get_logger("core_logger");
    if (logger) {
        // logger->debug("Loading embedded environment from {}", resource_path_);
    }

    std::string env_content = extract_env_content();
    parse_env(env_content);

    if (logger) {
        // logger->info("Embedded environment loaded from {}", resource_path_);
    }
}


std::string EmbeddedEnv::extract_env_content()
{
    QFile file(QString::fromStdString(resource_path_));
    if (!file.open(QIODevice::ReadOnly)) {
        const std::string error_message =
            "Failed to load embedded .env file from resource: " + resource_path_;
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("{}", error_message);
        }
        throw std::runtime_error(error_message);
    }

    const QByteArray data = file.readAll();
    return std::string(data.constData(), static_cast<std::size_t>(data.size()));
}


void EmbeddedEnv::parse_env(const std::string& env_content) {
    std::istringstream stream(env_content);
    std::string line;
    size_t loaded_entries = 0;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::size_t equal_pos = line.find('=');
        if (equal_pos == std::string::npos) {
            std::string message = "Invalid .env line: " + line;
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->warn("{}", message);
            }
            throw std::runtime_error(message);
        }

        std::string key = line.substr(0, equal_pos);
        std::string value = line.substr(equal_pos + 1);

        key = trim(key);
        value = trim(value);

        // Set the environment variable
#if defined(_WIN32)
        _putenv_s(key.c_str(), value.c_str());  // Windows-specific
#else
        setenv(key.c_str(), value.c_str(), 1);  // POSIX-compliant
#endif
        ++loaded_entries;
        if (auto logger = Logger::get_logger("core_logger")) {
            // logger->debug("Loaded env key '{}'", key);
        }
    }

    if (auto logger = Logger::get_logger("core_logger")) {
        logger->info("Loaded {} environment variable(s) from embedded resource", loaded_entries);
    }
}


std::string EmbeddedEnv::trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    std::size_t start = str.find_first_not_of(whitespace);
    std::size_t end = str.find_last_not_of(whitespace);

    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }

    return str.substr(start, end - start + 1);
}
