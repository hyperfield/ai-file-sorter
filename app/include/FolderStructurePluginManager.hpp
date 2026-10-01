#pragma once

#include "FolderStructurePluginProfile.hpp"

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Installs and loads signed declarative folder-structure plugins.
 */
class FolderStructurePluginManager {
public:
    /**
     * @brief Constructs a manager rooted at an application config directory.
     * @param config_dir Application config directory.
     * @param trusted_keys Trusted public keys. Empty uses compiled-in defaults.
     */
    explicit FolderStructurePluginManager(
        std::string config_dir,
        std::vector<FolderStructurePluginPublicKey> trusted_keys = {});

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
     * @brief Installs a signed folder-structure plugin archive.
     * @param archive_path Archive path to import.
     * @param installed_plugin_id Optional output for the installed plugin id.
     * @param error Optional output for a user-facing failure reason.
     * @return True when installation succeeds.
     */
    bool install_from_archive(const std::filesystem::path& archive_path,
                              std::string* installed_plugin_id = nullptr,
                              std::string* error = nullptr) const;

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
    std::vector<FolderStructurePluginPublicKey> trusted_keys() const;
    std::optional<FolderStructurePluginManifest> load_verified_manifest(
        const std::filesystem::path& package_dir,
        std::string* error = nullptr) const;

    std::string config_dir_;
    std::vector<FolderStructurePluginPublicKey> trusted_keys_;
};
