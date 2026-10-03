#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "PluginPackageFetcher.hpp"

/**
 * @brief App-side metadata for a folder-structure plugin listed by the website catalog.
 */
struct FolderStructurePluginCatalogEntry {
    /** @brief Stable plugin id. */
    std::string id;
    /** @brief User-facing plugin name. */
    std::string name;
    /** @brief User-facing plugin description. */
    std::string description;
    /** @brief Published package version. */
    std::string version;
    /** @brief True when the plugin requires a commercial entitlement before install/use. */
    bool license_required{false};
    /** @brief Commercial product id used by the activation endpoint. */
    std::string product_id;
    /** @brief Webpage where the user can obtain or manage the plugin. */
    std::string purchase_url;
    /** @brief Optional direct package URL for catalog-delivered archives. */
    std::string package_download_url;
    /** @brief Expected SHA-256 for package_download_url. */
    std::string package_sha256;

    /**
     * @brief Reports whether the entry has complete remote package metadata.
     * @return True when both package URL and expected SHA-256 are present.
     */
    bool has_remote_package() const { return !package_download_url.empty() && !package_sha256.empty(); }
};

/**
 * @brief Returns the default folder-structure plugin catalog URL.
 * @return Runtime environment override, compiled-in default, or an empty string.
 */
std::string default_folder_structure_plugin_catalog_url();

/**
 * @brief Parses one catalog entry JSON object.
 * @param json JSON object text.
 * @param error Optional output for a user-facing parse failure.
 * @return Parsed entry when valid.
 */
std::optional<FolderStructurePluginCatalogEntry> load_folder_structure_plugin_catalog_entry_from_json(
    const std::string& json, std::string* error = nullptr);

/**
 * @brief Parses a folder-structure plugin catalog document.
 * @param json Catalog JSON text.
 * @param error Optional output for a user-facing parse failure.
 * @return Valid catalog entries.
 */
std::vector<FolderStructurePluginCatalogEntry> load_folder_structure_plugin_catalog_entries_from_json(
    const std::string& json, std::string* error = nullptr);

/**
 * @brief Fetches the remote folder-structure plugin catalog and package archives.
 */
class FolderStructurePluginCatalogFetcher {
   public:
    using ProgressCallback = PluginPackageFetcher::ProgressCallback;
    using CancelCheck = PluginPackageFetcher::CancelCheck;
    using DownloadFunction = PluginPackageFetcher::DownloadFunction;

    /**
     * @brief Constructs a catalog fetcher rooted at a cache directory.
     * @param cache_directory Directory used for cached catalog and package data.
     * @param download_fn Optional download override, primarily for tests.
     */
    explicit FolderStructurePluginCatalogFetcher(std::filesystem::path cache_directory = {},
                                                 DownloadFunction download_fn = {});

    /**
     * @brief Downloads and parses a folder-structure plugin catalog.
     * @param catalog_url Catalog endpoint URL.
     * @param error Optional output for a user-facing failure reason.
     * @return Parsed catalog entries.
     */
    std::vector<FolderStructurePluginCatalogEntry> fetch_catalog_entries(const std::string& catalog_url,
                                                                         std::string* error = nullptr) const;

    /**
     * @brief Downloads and SHA-256 verifies a catalog entry's package archive.
     * @param entry Catalog entry with package metadata.
     * @param archive_path Output path to the cached archive.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the archive exists in cache and matches the expected SHA-256.
     */
    bool fetch_package_archive(const FolderStructurePluginCatalogEntry& entry, std::filesystem::path* archive_path,
                               std::string* error = nullptr) const;

    /**
     * @brief Returns the deterministic cache path for the catalog document.
     * @param cache_directory Root cache directory.
     * @return Cache path for the folder-structure plugin catalog.
     */
    static std::filesystem::path catalog_cache_path(const std::filesystem::path& cache_directory);

   private:
    std::filesystem::path cache_directory_;
    PluginPackageFetcher fetcher_;
};
