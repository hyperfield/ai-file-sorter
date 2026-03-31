#pragma once

#include "StoragePluginManifest.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

/**
 * @brief Downloads and verifies remote plugin manifests and package archives.
 */
class StoragePluginPackageFetcher {
public:
    using ProgressCallback = std::function<void(double, const std::string&)>;
    using CancelCheck = std::function<bool()>;
    using DownloadFunction = std::function<void(const std::string&,
                                                const std::filesystem::path&,
                                                ProgressCallback,
                                                CancelCheck)>;

    explicit StoragePluginPackageFetcher(std::filesystem::path cache_directory = {},
                                         DownloadFunction download_fn = {});

    std::vector<StoragePluginManifest> fetch_catalog_manifests(
        const std::string& catalog_url,
        std::string* error = nullptr) const;
    std::optional<StoragePluginManifest> fetch_remote_manifest(
        const StoragePluginManifest& manifest,
        std::string* error = nullptr) const;
    bool fetch_package_archive(const StoragePluginManifest& manifest,
                               std::filesystem::path* archive_path,
                               std::string* error = nullptr) const;

private:
    std::filesystem::path cache_directory_;
    DownloadFunction download_fn_;

    static std::filesystem::path manifest_cache_path(const std::filesystem::path& cache_directory,
                                                     const StoragePluginManifest& manifest);
    static std::filesystem::path catalog_cache_path(const std::filesystem::path& cache_directory);
    static std::filesystem::path package_cache_path(const std::filesystem::path& cache_directory,
                                                    const StoragePluginManifest& manifest);
    static std::string normalize_sha256(std::string value);
    static std::string safe_file_name_from_url(const std::string& url, std::string fallback_stem);
    static std::string compute_sha256(const std::filesystem::path& path);
    static bool verify_file(const std::filesystem::path& path, const std::string& expected_sha256);
    static void default_download(const std::string& url,
                                 const std::filesystem::path& destination_path,
                                 ProgressCallback progress_cb,
                                 CancelCheck cancel_check);
};
