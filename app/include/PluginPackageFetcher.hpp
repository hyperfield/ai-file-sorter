#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

/**
 * @brief Remote downloadable plugin package metadata shared by plugin domains.
 */
struct PluginPackageSource {
    /** @brief Stable plugin id used for cache partitioning. */
    std::string plugin_id;
    /** @brief Plugin version used for cache partitioning. */
    std::string version;
    /** @brief URL for the downloadable .aifsplugin archive. */
    std::string package_download_url;
    /** @brief Expected SHA-256 for the downloadable archive. */
    std::string package_sha256;
};

/**
 * @brief Downloads plugin catalogs, manifests, and package archives into a cache directory.
 */
class PluginPackageFetcher {
   public:
    using ProgressCallback = std::function<void(double, const std::string&)>;
    using CancelCheck = std::function<bool()>;
    using DownloadFunction =
        std::function<void(const std::string&, const std::filesystem::path&, ProgressCallback, CancelCheck)>;

    /**
     * @brief Constructs a fetcher rooted at a cache directory.
     * @param cache_directory Directory used for cached remote plugin data.
     * @param download_fn Optional download override, primarily for tests.
     */
    explicit PluginPackageFetcher(std::filesystem::path cache_directory = {}, DownloadFunction download_fn = {});

    /**
     * @brief Returns the configured cache directory.
     * @return Cache directory path.
     */
    const std::filesystem::path& cache_directory() const { return cache_directory_; }

    /**
     * @brief Downloads a URL into an atomic cache file.
     * @param url Source URL.
     * @param cache_path Final cache path to replace on success.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the URL was downloaded and finalized.
     */
    bool fetch_to_cache(const std::string& url, const std::filesystem::path& cache_path,
                        std::string* error = nullptr) const;

    /**
     * @brief Downloads a text URL into cache and returns the cached body.
     * @param url Source URL.
     * @param cache_path Final cache path to replace on success.
     * @param error Optional output for a user-facing failure reason.
     * @return Cached body when the download and read succeed.
     */
    std::optional<std::string> fetch_text(const std::string& url, const std::filesystem::path& cache_path,
                                          std::string* error = nullptr) const;

    /**
     * @brief Downloads and SHA-256 verifies a remote plugin archive.
     * @param source Remote package metadata.
     * @param archive_path Output path to the cached archive.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the archive exists in cache and matches the expected SHA-256.
     */
    bool fetch_package_archive(const PluginPackageSource& source, std::filesystem::path* archive_path,
                               std::string* error = nullptr) const;

    /**
     * @brief Returns the deterministic cache path for a package source.
     * @param cache_directory Root cache directory.
     * @param source Remote package metadata.
     * @return Cache path for the archive.
     */
    static std::filesystem::path package_cache_path(const std::filesystem::path& cache_directory,
                                                    const PluginPackageSource& source);

   private:
    std::filesystem::path cache_directory_;
    DownloadFunction download_fn_;

    static std::string normalize_sha256(std::string value);
    static std::string safe_file_name_from_url(const std::string& url, std::string fallback_stem);
    static std::string compute_sha256(const std::filesystem::path& path);
    static bool verify_file(const std::filesystem::path& path, const std::string& expected_sha256);
    static void default_download(const std::string& url, const std::filesystem::path& destination_path,
                                 ProgressCallback progress_cb, CancelCheck cancel_check);
};
