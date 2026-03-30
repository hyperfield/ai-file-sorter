#include "StoragePluginManifest.hpp"

namespace {

const std::vector<StoragePluginManifest>& manifest_catalog()
{
    static const std::vector<StoragePluginManifest> catalog = {
        StoragePluginManifest{
            .id = "cloud_storage_compat",
            .name = "Cloud Storage Compatibility",
            .description =
                "Adds compatibility providers for OneDrive, Dropbox, and pCloud. "
                "Installed providers use safer recursive scans and relaxed undo timestamp validation for synced folders.",
            .version = "1.0.0",
            .provider_ids = {"onedrive", "dropbox", "pcloud"}
        }
    };
    return catalog;
}

} // namespace

const std::vector<StoragePluginManifest>& builtin_storage_plugin_manifests()
{
    return manifest_catalog();
}

std::optional<StoragePluginManifest> find_storage_plugin_manifest(const std::string& plugin_id)
{
    for (const auto& manifest : manifest_catalog()) {
        if (manifest.id == plugin_id) {
            return manifest;
        }
    }
    return std::nullopt;
}

std::optional<StoragePluginManifest> find_storage_plugin_manifest_for_provider(const std::string& provider_id)
{
    for (const auto& manifest : manifest_catalog()) {
        for (const auto& supported_provider_id : manifest.provider_ids) {
            if (supported_provider_id == provider_id) {
                return manifest;
            }
        }
    }
    return std::nullopt;
}
