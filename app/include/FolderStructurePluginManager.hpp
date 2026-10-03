#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "FolderStructurePluginCatalog.hpp"
#include "FolderStructurePluginProfile.hpp"

class PluginEntitlementService;

/**
 * @brief Detailed result data for a failed folder-structure plugin installation.
 */
struct FolderStructurePluginInstallError {
    /** @brief User-facing failure message. */
    std::string message;
    /** @brief True when installation failed only because a commercial entitlement is missing. */
    bool missing_entitlement{false};
    /** @brief Product id that must be activated before installation can continue. */
    std::string product_id;
    /** @brief Plugin id from the signed manifest, when known. */
    std::string plugin_id;
    /** @brief User-facing plugin name from the signed manifest, when known. */
    std::string plugin_name;
    /** @brief Optional purchase or account-management URL from the signed manifest. */
    std::string purchase_url;
};

/**
 * @brief Installs and loads signed declarative folder-structure plugins.
 */
class FolderStructurePluginManager {
   public:
    /**
     * @brief Constructs a manager rooted at an application config directory.
     * @param config_dir Application config directory.
     * @param trusted_keys Trusted public keys. Empty uses compiled-in defaults.
     * @param entitlement_service Service used to validate premium plugin entitlements.
     */
    explicit FolderStructurePluginManager(std::string config_dir,
                                          std::vector<FolderStructurePluginPublicKey> trusted_keys = {},
                                          std::shared_ptr<const PluginEntitlementService> entitlement_service = nullptr,
                                          FolderStructurePluginCatalogFetcher::DownloadFunction download_fn = {});

    /**
     * @brief Returns the managed package directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path where installed folder-structure packages live.
     */
    static std::filesystem::path package_directory_for_config_dir(const std::string& config_dir);

    /**
     * @brief Returns the managed staging directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path used for temporary archive extraction.
     */
    static std::filesystem::path staging_directory_for_config_dir(const std::string& config_dir);
    /**
     * @brief Returns the managed remote-download cache directory for a config directory.
     * @param config_dir Application config directory.
     * @return Filesystem path where fetched catalogs and archives are cached.
     */
    static std::filesystem::path download_directory_for_config_dir(const std::string& config_dir);

    /**
     * @brief Returns the merged view of installed and remote-catalog folder-structure plugins.
     * @return Available folder-structure plugin catalog entries.
     */
    std::vector<FolderStructurePluginCatalogEntry> available_plugins() const;
    /**
     * @brief Looks up a folder-structure plugin by id across installed and catalog entries.
     * @param plugin_id Plugin identifier.
     * @return Matching plugin entry when available.
     */
    std::optional<FolderStructurePluginCatalogEntry> find_plugin(const std::string& plugin_id) const;
    /**
     * @brief Reports whether a remote folder-structure plugin catalog URL is configured.
     * @return True when a remote catalog source is available.
     */
    bool remote_catalog_configured() const;
    /**
     * @brief Refreshes the cached remote folder-structure plugin catalog.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the catalog was fetched and parsed.
     */
    bool refresh_remote_catalog(std::string* error = nullptr);
    /**
     * @brief Reports whether a catalog-delivered package can update an installed plugin.
     * @param plugin_id Plugin identifier.
     * @return True when the catalog advertises a newer package.
     */
    bool can_update(const std::string& plugin_id) const;

    /**
     * @brief Returns verified installed plugin manifests.
     * @return Installed plugin manifests for packages with valid signatures.
     */
    std::vector<FolderStructurePluginManifest> installed_plugins() const;

    /**
     * @brief Returns every verified installed folder-structure profile.
     * @return Installed profile descriptors usable by folder initialization and routing.
     */
    std::vector<FolderStructurePluginProfile> installed_profiles() const;

    /**
     * @brief Checks whether a plugin id is installed and verified.
     * @param plugin_id Plugin id to find.
     * @return True when a valid installed package exists.
     */
    bool is_installed(const std::string& plugin_id) const;
    /**
     * @brief Checks whether an installed plugin is enabled for templates and routing guidance.
     * @param plugin_id Plugin id to query.
     * @return True when the plugin is not explicitly disabled.
     */
    bool is_enabled(const std::string& plugin_id) const;
    /**
     * @brief Enables or disables an installed plugin.
     * @param plugin_id Plugin id to update.
     * @param enabled True to load the plugin, false to keep it installed but inactive.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the preference was saved.
     */
    bool set_enabled(const std::string& plugin_id, bool enabled, std::string* error = nullptr) const;

    /**
     * @brief Activates a commercial plugin license through the configured activation endpoint.
     * @param product_id Product id requested by a plugin manifest.
     * @param license_key User-provided license key.
     * @param error Optional output for a user-facing failure reason.
     * @return True when activation succeeded and a signed receipt was cached.
     */
    bool activate_license(const std::string& product_id, const std::string& license_key,
                          std::string* error = nullptr) const;

    /**
     * @brief Installs or updates a plugin from the remote catalog.
     * @param plugin_id Plugin identifier.
     * @param error Optional output for a user-facing failure reason.
     * @return True when installation succeeds.
     */
    bool install(const std::string& plugin_id, std::string* error = nullptr) const;
    /**
     * @brief Installs or updates a plugin from the remote catalog with structured failure details.
     * @param plugin_id Plugin identifier.
     * @param error Optional output for structured failure details.
     * @return True when installation succeeds.
     */
    bool install(const std::string& plugin_id, FolderStructurePluginInstallError* error) const;

    /**
     * @brief Installs a signed folder-structure plugin archive.
     * @param archive_path Archive path to import.
     * @param installed_plugin_id Optional output for the installed plugin id.
     * @param error Optional output for a user-facing failure reason.
     * @return True when installation succeeds.
     */
    bool install_from_archive(const std::filesystem::path& archive_path, std::string* installed_plugin_id = nullptr,
                              std::string* error = nullptr) const;

    /**
     * @brief Installs a signed folder-structure plugin archive with structured failure details.
     * @param archive_path Archive path to import.
     * @param installed_plugin_id Optional output for the installed plugin id.
     * @param error Optional output for structured failure details.
     * @return True when installation succeeds.
     */
    bool install_from_archive(const std::filesystem::path& archive_path, std::string* installed_plugin_id,
                              FolderStructurePluginInstallError* error) const;

    /**
     * @brief Removes an installed folder-structure plugin package.
     * @param plugin_id Plugin id to uninstall.
     * @param error Optional output for a user-facing failure reason.
     * @return True when plugin artifacts were removed or already absent.
     */
    bool uninstall(const std::string& plugin_id, std::string* error = nullptr) const;

   private:
    std::filesystem::path package_root() const;
    std::filesystem::path staging_root() const;
    std::filesystem::path state_file() const;
    std::vector<FolderStructurePluginPublicKey> trusted_keys() const;
    std::vector<std::string> disabled_plugin_ids() const;
    bool save_disabled_plugin_ids(std::vector<std::string> plugin_ids, std::string* error = nullptr) const;
    void load_cached_remote_catalog();
    std::optional<FolderStructurePluginCatalogEntry> remote_catalog_entry(const std::string& plugin_id) const;
    bool has_required_entitlement(const FolderStructurePluginCatalogEntry& entry,
                                  FolderStructurePluginInstallError* error) const;
    bool has_required_entitlement(const FolderStructurePluginManifest& manifest,
                                  FolderStructurePluginInstallError* error) const;
    bool has_required_entitlement(const FolderStructurePluginManifest& manifest, std::string* error = nullptr) const;
    bool install_from_archive_internal(const std::filesystem::path& archive_path,
                                       const FolderStructurePluginCatalogEntry* expected_catalog_entry,
                                       std::string* installed_plugin_id,
                                       FolderStructurePluginInstallError* error) const;
    std::optional<FolderStructurePluginManifest> load_verified_manifest(const std::filesystem::path& package_dir,
                                                                        std::string* error = nullptr) const;

    std::string config_dir_;
    std::vector<FolderStructurePluginPublicKey> trusted_keys_;
    std::shared_ptr<const PluginEntitlementService> entitlement_service_;
    FolderStructurePluginCatalogFetcher catalog_fetcher_;
    std::string remote_catalog_url_;
    std::vector<FolderStructurePluginCatalogEntry> remote_catalog_entries_;
    mutable std::recursive_mutex mutex_;
};
