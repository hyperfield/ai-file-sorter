#include "StoragePluginPackageFetcher.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

StoragePluginPackageFetcher::StoragePluginPackageFetcher(std::filesystem::path cache_directory,
                                                         DownloadFunction download_fn)
    : cache_directory_(std::move(cache_directory)), fetcher_(cache_directory_, std::move(download_fn)) {}

std::vector<StoragePluginManifest> StoragePluginPackageFetcher::fetch_catalog_manifests(const std::string& catalog_url,
                                                                                        std::string* error) const {
    if (catalog_url.empty()) {
        if (error) {
            *error = "No plugin catalog URL is configured.";
        }
        return {};
    }

    auto catalog_json = fetcher_.fetch_text(catalog_url, catalog_cache_path(cache_directory_), error);
    if (!catalog_json.has_value()) {
        return {};
    }
    return load_storage_plugin_manifests_from_json(*catalog_json, error);
}

std::optional<StoragePluginManifest> StoragePluginPackageFetcher::fetch_remote_manifest(
    const StoragePluginManifest& manifest, std::string* error) const {
    if (manifest.remote_manifest_url.empty()) {
        if (error) {
            *error = "No remote manifest URL is configured for this plugin.";
        }
        return std::nullopt;
    }

    const auto manifest_path = manifest_cache_path(cache_directory_, manifest);
    if (!fetcher_.fetch_to_cache(manifest.remote_manifest_url, manifest_path, error)) {
        return std::nullopt;
    }

    std::string manifest_error;
    auto remote_manifest = load_storage_plugin_manifest_from_file(manifest_path, &manifest_error);
    if (!remote_manifest.has_value()) {
        if (error) {
            *error = manifest_error.empty() ? "Failed to parse downloaded plugin manifest." : manifest_error;
        }
        return std::nullopt;
    }

    if (remote_manifest->id != manifest.id) {
        if (error) {
            *error = "Downloaded plugin manifest does not match the requested plugin id.";
        }
        return std::nullopt;
    }

    return remote_manifest;
}

bool StoragePluginPackageFetcher::fetch_package_archive(const StoragePluginManifest& manifest,
                                                        std::filesystem::path* archive_path, std::string* error) const {
    const PluginPackageSource source{.plugin_id = manifest.id,
                                     .version = manifest.version,
                                     .package_download_url = manifest.package_download_url,
                                     .package_sha256 = manifest.package_sha256};
    return fetcher_.fetch_package_archive(source, archive_path, error);
}

std::filesystem::path StoragePluginPackageFetcher::manifest_cache_path(const std::filesystem::path& cache_directory,
                                                                       const StoragePluginManifest& manifest) {
    return cache_directory / manifest.id / "remote-manifest.json";
}

std::filesystem::path StoragePluginPackageFetcher::catalog_cache_path(const std::filesystem::path& cache_directory) {
    return cache_directory / "catalog" / "storage-plugin-catalog.json";
}
