#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "PluginPackageFetcher.hpp"
#include "StoragePluginManifest.hpp"

/**
 * @brief Downloads and verifies remote plugin manifests and package archives.
 */
class StoragePluginPackageFetcher {
   public:
    using ProgressCallback = PluginPackageFetcher::ProgressCallback;
    using CancelCheck = PluginPackageFetcher::CancelCheck;
    using DownloadFunction = PluginPackageFetcher::DownloadFunction;

    explicit StoragePluginPackageFetcher(std::filesystem::path cache_directory = {}, DownloadFunction download_fn = {});

    std::vector<StoragePluginManifest> fetch_catalog_manifests(const std::string& catalog_url,
                                                               std::string* error = nullptr) const;
    std::optional<StoragePluginManifest> fetch_remote_manifest(const StoragePluginManifest& manifest,
                                                               std::string* error = nullptr) const;
    bool fetch_package_archive(const StoragePluginManifest& manifest, std::filesystem::path* archive_path,
                               std::string* error = nullptr) const;

   private:
    std::filesystem::path cache_directory_;
    PluginPackageFetcher fetcher_;

    static std::filesystem::path manifest_cache_path(const std::filesystem::path& cache_directory,
                                                     const StoragePluginManifest& manifest);
    static std::filesystem::path catalog_cache_path(const std::filesystem::path& cache_directory);
};
