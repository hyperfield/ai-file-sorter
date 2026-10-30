#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Declares a storage plugin package that can provide one or more storage providers.
 */
struct StoragePluginManifest {
    /** @brief Stable plugin id. */
    std::string id;
    /** @brief User-facing plugin name. */
    std::string name;
    /** @brief User-facing plugin description. */
    std::string description;
    /** @brief Plugin package version. */
    std::string version;
    /** @brief Storage provider ids contributed by this plugin. */
    std::vector<std::string> provider_ids;
    /** @brief Optional platform constraints. Empty means any platform. */
    std::vector<std::string> platforms;
    /** @brief Optional architecture constraints. Empty means any architecture. */
    std::vector<std::string> architectures;
    /** @brief Optional URL for remote manifest metadata. */
    std::string remote_manifest_url;
    /** @brief Optional URL for the downloadable .aifsplugin archive. */
    std::string package_download_url;
    /** @brief Expected SHA-256 for the downloadable archive. */
    std::string package_sha256;
    /** @brief Host-recognized entry point kind, for example external_process. */
    std::string entry_point_kind;
    /** @brief Connector executable path or built-in entry point key. */
    std::string entry_point;
    /** @brief Package-relative files or directories copied for external process plugins. */
    std::vector<std::string> package_paths;
    /** @brief True when the plugin requires a separate entitlement receipt before use. */
    bool license_required{false};
    /** @brief Commercial product id checked against local entitlement receipts. */
    std::string product_id;
    /** @brief Optional webpage where the user can purchase or manage the entitlement. */
    std::string purchase_url;
    /** @brief Manifest path inside an extracted, installed, or cached package. */
    std::filesystem::path source_path;
    /** @brief Trusted key id that verified the storage package archive. */
    std::string verified_signer_key_id;

    bool has_remote_manifest() const { return !remote_manifest_url.empty(); }

    bool has_remote_package() const { return !package_download_url.empty() && !package_sha256.empty(); }
};

const std::vector<StoragePluginManifest>& builtin_storage_plugin_manifests();
std::optional<StoragePluginManifest> find_storage_plugin_manifest(const std::string& plugin_id);
std::optional<StoragePluginManifest> find_storage_plugin_manifest_for_provider(const std::string& provider_id);
std::string storage_plugin_current_platform();
std::string storage_plugin_current_architecture();
bool storage_plugin_manifest_matches_current_runtime(const StoragePluginManifest& manifest,
                                                     std::string* error = nullptr);
std::optional<StoragePluginManifest> load_storage_plugin_manifest_from_file(const std::filesystem::path& manifest_path,
                                                                            std::string* error = nullptr);
std::optional<StoragePluginManifest> load_storage_plugin_manifest_from_json(const std::string& json,
                                                                            std::string* error = nullptr);
std::vector<StoragePluginManifest> load_storage_plugin_manifests_from_directory(
    const std::filesystem::path& manifest_directory, std::string* error = nullptr);
std::vector<StoragePluginManifest> load_storage_plugin_manifests_from_json(const std::string& json,
                                                                           std::string* error = nullptr);
bool save_storage_plugin_manifest_to_file(const StoragePluginManifest& manifest,
                                          const std::filesystem::path& manifest_path, std::string* error = nullptr);
