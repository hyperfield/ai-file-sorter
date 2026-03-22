#include "UpdaterLiveTestConfig.hpp"

#include "IniConfig.hpp"
#include "Utils.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

std::string trim_copy(const std::string& value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::optional<std::string> read_ini_value(const IniConfig& config,
                                          const std::string& section,
                                          const std::initializer_list<const char*>& keys)
{
    for (const char* key : keys) {
        if (!config.hasValue(section, key)) {
            continue;
        }
        const std::string value = trim_copy(config.getValue(section, key));
        if (!value.empty()) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> read_live_test_value(const IniConfig& config,
                                                const std::initializer_list<const char*>& keys)
{
    if (auto value = read_ini_value(config, "LiveTest", keys)) {
        return value;
    }
    return read_ini_value(config, "", keys);
}

} // namespace

std::optional<std::filesystem::path> find_updater_live_test_ini(const std::filesystem::path& executable_path)
{
    std::filesystem::path base_dir = executable_path;
    if (!base_dir.empty()) {
        if (!base_dir.has_filename() || base_dir.extension().empty()) {
            // Treat an extensionless path as a directory candidate.
        } else {
            base_dir = base_dir.parent_path();
        }
    }

    if (base_dir.empty()) {
        return std::nullopt;
    }

    const auto candidate = base_dir / "live-test.ini";
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec) && !ec) {
        return candidate;
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> load_missing_values_from_live_test_ini(
    UpdaterLiveTestConfig& config,
    const std::filesystem::path& executable_path)
{
    if (!config.enabled) {
        return std::nullopt;
    }

    const auto ini_path = find_updater_live_test_ini(executable_path);
    if (!ini_path) {
        return std::nullopt;
    }

    IniConfig ini;
    if (!ini.load(Utils::path_to_utf8(*ini_path))) {
        throw std::runtime_error("Failed to load updater live test file: " + Utils::path_to_utf8(*ini_path));
    }

    if (!config.installer_url) {
        config.installer_url = read_live_test_value(ini, {"download_url", "installer_url", "url"});
    }
    if (!config.installer_sha256) {
        config.installer_sha256 = read_live_test_value(ini, {"sha256", "installer_sha256"});
    }
    if (!config.current_version) {
        config.current_version = read_live_test_value(ini, {"current_version"});
    }
    if (!config.min_version) {
        config.min_version = read_live_test_value(ini, {"min_version"});
    }

    return ini_path;
}
