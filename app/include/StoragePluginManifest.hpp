#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Declares a storage plugin package that can provide one or more storage providers.
 */
struct StoragePluginManifest {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::vector<std::string> provider_ids;
    std::vector<std::string> platforms;
    std::vector<std::string> architectures;
    std::string remote_manifest_url;
    std::string package_download_url;
    std::string package_sha256;
    std::string entry_point_kind;
    std::string entry_point;
    std::vector<std::string> package_paths;
    std::filesystem::path source_path;

    bool has_remote_manifest() const
    {
        return !remote_manifest_url.empty();
    }

    bool has_remote_package() const
    {
        return !package_download_url.empty() && !package_sha256.empty();
    }
};

const std::vector<StoragePluginManifest>& builtin_storage_plugin_manifests();
std::optional<StoragePluginManifest> find_storage_plugin_manifest(const std::string& plugin_id);
std::optional<StoragePluginManifest> find_storage_plugin_manifest_for_provider(const std::string& provider_id);
std::string storage_plugin_current_platform();
std::string storage_plugin_current_architecture();
bool storage_plugin_manifest_matches_current_runtime(const StoragePluginManifest& manifest,
                                                     std::string* error = nullptr);
std::optional<StoragePluginManifest> load_storage_plugin_manifest_from_file(
    const std::filesystem::path& manifest_path,
    std::string* error = nullptr);
std::optional<StoragePluginManifest> load_storage_plugin_manifest_from_json(
    const std::string& json,
    std::string* error = nullptr);
std::vector<StoragePluginManifest> load_storage_plugin_manifests_from_directory(
    const std::filesystem::path& manifest_directory,
    std::string* error = nullptr);
std::vector<StoragePluginManifest> load_storage_plugin_manifests_from_json(
    const std::string& json,
    std::string* error = nullptr);
bool save_storage_plugin_manifest_to_file(
    const StoragePluginManifest& manifest,
    const std::filesystem::path& manifest_path,
    std::string* error = nullptr);
