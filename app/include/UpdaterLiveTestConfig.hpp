#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct UpdaterLiveTestConfig
{
    bool enabled{false};
    std::optional<std::string> installer_url;
    std::optional<std::string> installer_sha256;
    std::optional<std::string> current_version;
    std::optional<std::string> min_version;
};

std::optional<std::filesystem::path> find_updater_live_test_ini(const std::filesystem::path& executable_path);
std::optional<std::filesystem::path> load_missing_values_from_live_test_ini(
    UpdaterLiveTestConfig& config,
    const std::filesystem::path& executable_path);
