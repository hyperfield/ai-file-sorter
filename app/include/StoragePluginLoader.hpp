#pragma once

#include "StoragePluginEntryPointRegistry.hpp"
#include "StoragePluginManifest.hpp"
#include "StorageProvider.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Constructs provider instances described by installed storage plugin manifests.
 */
class StoragePluginLoader {
public:
    explicit StoragePluginLoader(std::filesystem::path manifest_directory = {});

    std::vector<StoragePluginManifest> available_plugins() const;
    std::optional<StoragePluginManifest> find_plugin(const std::string& plugin_id) const;
    std::optional<StoragePluginManifest> find_plugin_for_provider(const std::string& provider_id) const;
    const std::filesystem::path& manifest_directory() const;
    bool supports_plugin(const StoragePluginManifest& manifest,
                         std::string* error = nullptr) const;

    std::vector<std::shared_ptr<IStorageProvider>> create_detection_providers() const;
    /**
     * @brief Creates runtime providers from the plugin manager's verified installed manifests.
     * @param installed_plugins Manifests authorized for runtime use.
     * @return Runtime providers declared by those manifests.
     */
    std::vector<std::shared_ptr<IStorageProvider>> create_providers_for_installed_plugins(
        const std::vector<StoragePluginManifest>& installed_plugins) const;

private:
    std::filesystem::path manifest_directory_;
    StoragePluginEntryPointRegistry entry_point_registry_;
};
