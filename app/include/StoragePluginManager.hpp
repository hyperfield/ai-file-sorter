#pragma once

#include "StoragePluginPackageFetcher.hpp"
#include "StoragePluginLoader.hpp"
#include "StoragePluginManifest.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Tracks which optional storage-provider plugins are installed for the app.
 */
class StoragePluginManager {
public:
    /**
     * @brief Persistent record of an installed plugin version.
     */
    struct InstalledPluginRecord {
        std::string id;
        std::string version;
    };

    /**
     * @brief Constructs a manager rooted at an application config directory.
     * @param config_dir Application config directory used for manifests, caches, and install state.
     * @param download_fn Optional fetch override used primarily by tests.
     */
    explicit StoragePluginManager(
        std::string config_dir,
        StoragePluginPackageFetcher::DownloadFunction download_fn = {});

    /**
     * @brief Returns the managed manifest directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path where installed plugin manifests are stored.
     */
    static std::filesystem::path manifest_directory_for_config_dir(const std::string& config_dir);
    /**
     * @brief Returns the managed catalog cache directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path where fetched plugin catalogs are cached.
     */
    static std::filesystem::path catalog_directory_for_config_dir(const std::string& config_dir);
    /**
     * @brief Returns the managed package directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path where installed plugin packages are materialized.
     */
    static std::filesystem::path package_directory_for_config_dir(const std::string& config_dir);
    /**
     * @brief Returns the managed download cache directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path where downloaded plugin archives and manifests are cached.
     */
    static std::filesystem::path download_directory_for_config_dir(const std::string& config_dir);

    /**
     * @brief Returns the merged view of built-in, installed, and remote-catalog plugin entries.
     * @return Available plugin manifests for the current runtime.
     */
    std::vector<StoragePluginManifest> available_plugins() const;
    /**
     * @brief Looks up a plugin manifest by plugin id.
     * @param plugin_id Plugin identifier.
     * @return Matching plugin manifest when available.
     */
    std::optional<StoragePluginManifest> find_plugin(const std::string& plugin_id) const;
    /**
     * @brief Resolves the plugin manifest responsible for a provider id.
     * @param provider_id Storage provider identifier.
     * @return Matching plugin manifest when available.
     */
    std::optional<StoragePluginManifest> find_plugin_for_provider(const std::string& provider_id) const;
    /**
     * @brief Reports whether a remote plugin catalog URL is configured.
     * @return True when a remote catalog source is available.
     */
    bool remote_catalog_configured() const;
    /**
     * @brief Reports whether the manager can attempt a remote refresh.
     * @return True when a remote catalog or remote plugin manifest source is configured.
     */
    bool can_check_for_updates() const;
    /**
     * @brief Refreshes the cached remote plugin catalog and installed plugin metadata.
     * @param error Optional output for a user-facing failure description.
     * @return True when new remote metadata was fetched and persisted.
     */
    bool refresh_remote_catalog(std::string* error = nullptr);
    /**
     * @brief Reports whether the current app build can execute the plugin entry point.
     * @param plugin_id Plugin identifier.
     * @return True when the plugin can be used by this build.
     */
    bool supports_plugin(const std::string& plugin_id) const;
    /**
     * @brief Reports whether a plugin is currently installed.
     * @param plugin_id Plugin identifier.
     * @return True when the plugin is installed locally.
     */
    bool is_installed(const std::string& plugin_id) const;
    /**
     * @brief Reports whether a newer plugin package is available.
     * @param plugin_id Plugin identifier.
     * @return True when an installed plugin can be updated from the configured source.
     */
    bool can_update(const std::string& plugin_id) const;
    /**
     * @brief Returns all installed plugin ids.
     * @return Installed plugin id list.
     */
    std::vector<std::string> installed_plugin_ids() const;
    /**
     * @brief Installs a plugin by id from local or remote sources.
     * @param plugin_id Plugin identifier.
     * @param error Optional output for a user-facing failure description.
     * @return True when the install completed successfully.
     */
    bool install(const std::string& plugin_id, std::string* error = nullptr);
    /**
     * @brief Updates an installed plugin from its configured remote source.
     * @param plugin_id Plugin identifier.
     * @param error Optional output for a user-facing failure description.
     * @return True when the update completed successfully.
     */
    bool update(const std::string& plugin_id, std::string* error = nullptr);
    /**
     * @brief Installs a plugin from a local archive file.
     * @param archive_path Archive path to import.
     * @param installed_plugin_id Optional output for the resolved plugin id.
     * @param error Optional output for a user-facing failure description.
     * @return True when the archive installed successfully.
     */
    bool install_from_archive(const std::filesystem::path& archive_path,
                              std::string* installed_plugin_id = nullptr,
                              std::string* error = nullptr);
    /**
     * @brief Uninstalls a plugin and removes its managed artifacts.
     * @param plugin_id Plugin identifier.
     * @param error Optional output for a user-facing failure description.
     * @return True when the uninstall completed successfully.
     */
    bool uninstall(const std::string& plugin_id, std::string* error = nullptr);
    /**
     * @brief Reloads installed manifests and cached remote metadata from disk.
     * @param error Optional output for a user-facing failure description.
     * @return True when the manager reloaded successfully.
     */
    bool reload(std::string* error = nullptr);

private:
    /**
     * @brief Returns the JSON file that persists installed-plugin state.
     * @return Full path to the install-state JSON file.
     */
    std::string install_state_path() const;
    /**
     * @brief Returns the managed manifest path for a plugin id.
     * @param plugin_id Plugin identifier.
     * @return Filesystem path to the installed manifest file.
     */
    std::filesystem::path manifest_path_for_plugin(const std::string& plugin_id) const;
    /**
     * @brief Returns the managed package directory for a manifest.
     * @param manifest Plugin manifest being materialized.
     * @return Filesystem directory containing packaged plugin assets.
     */
    std::filesystem::path package_directory_for_plugin(const StoragePluginManifest& manifest) const;
    /**
     * @brief Builds the merged plugin view across built-in, installed, and remote entries.
     * @return Available plugin manifests for the current runtime.
     */
    std::vector<StoragePluginManifest> merged_available_plugins() const;
    /**
     * @brief Loads any cached remote catalog metadata from disk.
     */
    void load_cached_remote_catalog();
    /**
     * @brief Persists the fetched remote catalog to disk.
     * @param manifests Remote catalog manifests to persist.
     * @param error Optional output for a user-facing failure description.
     * @return True when the catalog cache was saved successfully.
     */
    bool persist_remote_catalog(std::vector<StoragePluginManifest> manifests,
                                std::string* error = nullptr);
    /**
     * @brief Resolves the manifest to install after local/remote indirection.
     * @param manifest Candidate manifest selected by the user.
     * @param error Optional output for a user-facing failure description.
     * @return Materialized install manifest when resolution succeeds.
     */
    std::optional<StoragePluginManifest> resolve_install_manifest(
        const StoragePluginManifest& manifest,
        std::string* error = nullptr) const;
    /**
     * @brief Downloads and installs a remote plugin package.
     * @param manifest Remote plugin manifest describing the package source.
     * @param error Optional output for a user-facing failure description.
     * @return True when the remote package installs successfully.
     */
    bool install_from_remote_package(const StoragePluginManifest& manifest,
                                     std::string* error = nullptr);
    /**
     * @brief Installs a plugin from an archive with optional manifest expectations.
     * @param archive_path Local archive path.
     * @param expected_manifest Optional manifest the archive must satisfy.
     * @param installed_plugin_id Optional output for the resolved plugin id.
     * @param error Optional output for a user-facing failure description.
     * @return True when the archive installs successfully.
     */
    bool install_from_archive_internal(const std::filesystem::path& archive_path,
                                       const StoragePluginManifest* expected_manifest,
                                       std::string* installed_plugin_id,
                                       std::string* error = nullptr);
    /**
     * @brief Installs a fully materialized manifest into managed state.
     * @param manifest Plugin manifest to persist and activate.
     * @param error Optional output for a user-facing failure description.
     * @return True when the manifest installs successfully.
     */
    bool install_manifest(const StoragePluginManifest& manifest, std::string* error = nullptr);
    /**
     * @brief Persists a manifest into the managed manifest directory.
     * @param manifest Plugin manifest to persist.
     * @param error Optional output for a user-facing failure description.
     * @return True when the manifest was saved successfully.
     */
    bool persist_manifest(const StoragePluginManifest& manifest, std::string* error = nullptr) const;
    /**
     * @brief Rewrites a manifest so its entry points and assets point at managed package paths.
     * @param manifest Source manifest being installed.
     * @param materialized_manifest Output manifest rewritten for managed paths.
     * @param error Optional output for a user-facing failure description.
     * @return True when the manifest was materialized successfully.
     */
    bool materialize_manifest_for_install(const StoragePluginManifest& manifest,
                                          StoragePluginManifest* materialized_manifest,
                                          std::string* error = nullptr) const;
    /**
     * @brief Removes managed manifests and package artifacts for an installed plugin.
     * @param plugin_id Plugin identifier.
     * @param error Optional output for a user-facing failure description.
     * @return True when managed artifacts were removed successfully.
     */
    bool remove_plugin_artifacts(const std::string& plugin_id, std::string* error = nullptr) const;
    /**
     * @brief Persists the installed-plugin state file.
     * @param error Optional output for a user-facing failure description.
     * @return True when the state file was written successfully.
     */
    bool save(std::string* error = nullptr) const;

    std::string config_dir_;
    StoragePluginLoader loader_;
    StoragePluginPackageFetcher package_fetcher_;
    std::string remote_catalog_url_;
    std::unordered_map<std::string, InstalledPluginRecord> installed_plugins_;
    std::vector<StoragePluginManifest> remote_catalog_manifests_;
    mutable std::recursive_mutex mutex_;
};
