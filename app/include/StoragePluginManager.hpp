#pragma once

#include "StoragePluginPackageFetcher.hpp"
#include "StoragePluginLoader.hpp"
#include "StoragePluginManifest.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Tracks which optional storage-provider plugins are installed for the app.
 */
class StoragePluginManager {
public:
    struct InstalledPluginRecord {
        std::string id;
        std::string version;
    };

    explicit StoragePluginManager(
        std::string config_dir,
        StoragePluginPackageFetcher::DownloadFunction download_fn = {});

    static std::filesystem::path manifest_directory_for_config_dir(const std::string& config_dir);
    static std::filesystem::path catalog_directory_for_config_dir(const std::string& config_dir);
    static std::filesystem::path package_directory_for_config_dir(const std::string& config_dir);
    static std::filesystem::path download_directory_for_config_dir(const std::string& config_dir);

    std::vector<StoragePluginManifest> available_plugins() const;
    std::optional<StoragePluginManifest> find_plugin(const std::string& plugin_id) const;
    std::optional<StoragePluginManifest> find_plugin_for_provider(const std::string& provider_id) const;
    bool remote_catalog_configured() const;
    bool can_check_for_updates() const;
    bool refresh_remote_catalog(std::string* error = nullptr);
    bool supports_plugin(const std::string& plugin_id) const;
    bool is_installed(const std::string& plugin_id) const;
    bool can_update(const std::string& plugin_id) const;
    std::vector<std::string> installed_plugin_ids() const;
    bool install(const std::string& plugin_id, std::string* error = nullptr);
    bool update(const std::string& plugin_id, std::string* error = nullptr);
    bool install_from_archive(const std::filesystem::path& archive_path,
                              std::string* installed_plugin_id = nullptr,
                              std::string* error = nullptr);
    bool uninstall(const std::string& plugin_id, std::string* error = nullptr);
    bool reload(std::string* error = nullptr);

private:
    std::string install_state_path() const;
    std::filesystem::path manifest_path_for_plugin(const std::string& plugin_id) const;
    std::filesystem::path package_directory_for_plugin(const StoragePluginManifest& manifest) const;
    std::vector<StoragePluginManifest> merged_available_plugins() const;
    void load_cached_remote_catalog();
    bool persist_remote_catalog(std::vector<StoragePluginManifest> manifests,
                                std::string* error = nullptr);
    std::optional<StoragePluginManifest> resolve_install_manifest(
        const StoragePluginManifest& manifest,
        std::string* error = nullptr) const;
    bool install_from_remote_package(const StoragePluginManifest& manifest,
                                     std::string* error = nullptr);
    bool install_from_archive_internal(const std::filesystem::path& archive_path,
                                       const StoragePluginManifest* expected_manifest,
                                       std::string* installed_plugin_id,
                                       std::string* error = nullptr);
    bool install_manifest(const StoragePluginManifest& manifest, std::string* error = nullptr);
    bool persist_manifest(const StoragePluginManifest& manifest, std::string* error = nullptr) const;
    bool materialize_manifest_for_install(const StoragePluginManifest& manifest,
                                          StoragePluginManifest* materialized_manifest,
                                          std::string* error = nullptr) const;
    bool remove_plugin_artifacts(const std::string& plugin_id, std::string* error = nullptr) const;
    bool save(std::string* error = nullptr) const;

    std::string config_dir_;
    StoragePluginLoader loader_;
    StoragePluginPackageFetcher package_fetcher_;
    std::string remote_catalog_url_;
    std::unordered_map<std::string, InstalledPluginRecord> installed_plugins_;
    std::vector<StoragePluginManifest> remote_catalog_manifests_;
};
