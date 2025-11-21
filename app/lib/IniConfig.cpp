#include "IniConfig.hpp"
#include "Logger.hpp"
#include <cstdio>
#include <iostream>
#include <optional>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace {
template <typename... Args>
void ini_log(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}

std::string trim_copy(const std::string& input)
{
    const auto begin = input.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = input.find_last_not_of(" \t");
    return input.substr(begin, end - begin + 1);
}

bool should_skip_line(const std::string& line)
{
    return line.empty() || line.front() == ';' || line.front() == '#';
}

bool parse_section_header(const std::string& line, std::string& section)
{
    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
        section = line.substr(1, line.size() - 2);
        return true;
    }
    return false;
}

std::optional<std::pair<std::string, std::string>> parse_key_value(const std::string& line)
{
    const auto delimiter = line.find('=');
    if (delimiter == std::string::npos) {
        return std::nullopt;
    }
    std::string key = trim_copy(line.substr(0, delimiter));
    std::string value = trim_copy(line.substr(delimiter + 1));
    if (key.empty()) {
        return std::nullopt;
    }
    return std::make_pair(std::move(key), std::move(value));
}
}


bool IniConfig::load(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        ini_log(spdlog::level::err, "Failed to open config file: {}", filename);
        return false;
    }

    std::string raw_line;
    std::string section;
    while (std::getline(file, raw_line)) {
        const std::string line = trim_copy(raw_line);
        if (should_skip_line(line)) {
            continue;
        }
        if (parse_section_header(line, section)) {
            continue;
        }
        if (auto key_value = parse_key_value(line)) {
            data[section][key_value->first] = key_value->second;
        }
    }
    return true;
}


std::string IniConfig::getValue(const std::string &section, const std::string &key, const std::string &default_value) const {
    auto sec_it = data.find(section);
    if (sec_it != data.end()) {
        auto key_it = sec_it->second.find(key);
        if (key_it != sec_it->second.end()) {
            return key_it->second;
        }
    }
    return default_value;
}


void IniConfig::setValue(const std::string &section, const std::string &key, const std::string &value) {
    data[section][key] = value;
}


bool IniConfig::save(const std::string &filename) const
{
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        ini_log(spdlog::level::err, "Failed to open config file: {}", filename);
        return false;
    }

    for (const auto &section : data) {
        file << "[" << section.first << "]\n";
        for (const auto &pair : section.second) {
            file << pair.first << " = " << pair.second << "\n";
        }
        file << "\n";
    }

    return true;
}

bool IniConfig::hasValue(const std::string& section, const std::string& key) const
{
    const auto sec_it = data.find(section);
    if (sec_it == data.end()) {
        return false;
    }
    const auto key_it = sec_it->second.find(key);
    return key_it != sec_it->second.end();
}
