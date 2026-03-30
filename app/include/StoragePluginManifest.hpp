#pragma once

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
};

const std::vector<StoragePluginManifest>& builtin_storage_plugin_manifests();
std::optional<StoragePluginManifest> find_storage_plugin_manifest(const std::string& plugin_id);
std::optional<StoragePluginManifest> find_storage_plugin_manifest_for_provider(const std::string& provider_id);
