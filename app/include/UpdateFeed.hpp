#pragma once

#include <optional>
#include <string>

struct UpdateInfo
{
    std::string current_version;
    std::string min_version{"0.0.0"};
    std::string download_url;
    std::string release_notes_url;
    std::string installer_url;
    std::string installer_sha256;

    bool has_download_target() const
    {
        return !download_url.empty() || !installer_url.empty();
    }

    bool has_direct_installer() const
    {
        return !installer_url.empty();
    }
};

class UpdateFeed
{
public:
    enum class Platform {
        Windows,
        MacOS,
        Linux
    };

    static Platform current_platform();
    static std::optional<UpdateInfo> parse_for_current_platform(const std::string& update_json);
    static std::optional<UpdateInfo> parse_for_platform(const std::string& update_json, Platform platform);
};
