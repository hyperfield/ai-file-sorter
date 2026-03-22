#include "UpdateFeed.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>

#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

namespace {

std::string trim_copy(const std::string& value)
{
    auto trimmed = value;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    return trimmed;
}

std::string normalized_sha256(std::string value)
{
    value = trim_copy(value);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const char* platform_key(UpdateFeed::Platform platform)
{
    switch (platform) {
        case UpdateFeed::Platform::Windows: return "windows";
        case UpdateFeed::Platform::MacOS: return "macos";
        case UpdateFeed::Platform::Linux: return "linux";
    }
    return "linux";
}

bool has_platform_streams(const Json::Value& update)
{
    return (update.isMember("windows") && update["windows"].isObject())
        || (update.isMember("macos") && update["macos"].isObject())
        || (update.isMember("linux") && update["linux"].isObject());
}

const Json::Value* resolve_stream_root(const Json::Value& update, UpdateFeed::Platform platform)
{
    const char* key = platform_key(platform);

    if (update.isMember("streams") && update["streams"].isObject()) {
        const Json::Value& streams = update["streams"];
        if (streams.isMember(key) && streams[key].isObject()) {
            return &streams[key];
        }
        if (has_platform_streams(streams)) {
            return nullptr;
        }
    }

    if (update.isMember(key) && update[key].isObject()) {
        return &update[key];
    }
    if (has_platform_streams(update)) {
        return nullptr;
    }

    if (update.isObject()) {
        return &update;
    }

    return nullptr;
}

std::string string_member(const Json::Value& object, const char* key)
{
    if (object.isMember(key) && object[key].isString()) {
        return trim_copy(object[key].asString());
    }
    return {};
}

} // namespace

UpdateFeed::Platform UpdateFeed::current_platform()
{
#if defined(_WIN32)
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::MacOS;
#else
    return Platform::Linux;
#endif
}

std::optional<UpdateInfo> UpdateFeed::parse_for_current_platform(const std::string& update_json)
{
    return parse_for_platform(update_json, current_platform());
}

std::optional<UpdateInfo> UpdateFeed::parse_for_platform(const std::string& update_json, Platform platform)
{
    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::string errors;

    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    if (!reader->parse(update_json.c_str(),
                       update_json.c_str() + update_json.length(),
                       &root,
                       &errors)) {
        throw std::runtime_error("JSON Parse Error: " + errors);
    }

    if (!root.isMember("update") || !root["update"].isObject()) {
        return std::nullopt;
    }

    const Json::Value& update = root["update"];
    const Json::Value* stream = resolve_stream_root(update, platform);
    if (!stream || !stream->isObject()) {
        return std::nullopt;
    }

    UpdateInfo info;
    info.current_version = string_member(*stream, "current_version");
    info.min_version = string_member(*stream, "min_version");
    info.download_url = string_member(*stream, "download_url");
    info.release_notes_url = string_member(*stream, "release_notes_url");
    info.installer_url = string_member(*stream, "installer_url");
    info.installer_sha256 = normalized_sha256(string_member(*stream, "installer_sha256"));

    if (info.min_version.empty()) {
        info.min_version = "0.0.0";
    }
    if (info.download_url.empty() && !info.installer_url.empty()) {
        info.download_url = info.installer_url;
    }

    if (info.current_version.empty() || !info.has_download_target()) {
        return std::nullopt;
    }

    return info;
}
