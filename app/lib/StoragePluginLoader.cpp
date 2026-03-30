#include "StoragePluginLoader.hpp"

#include "CloudCompatibilityProvider.hpp"
#include "CloudPathDetectorProvider.hpp"

namespace {

std::shared_ptr<IStorageProvider> make_onedrive_detector()
{
    return std::make_shared<CloudPathDetectorProvider>(
        "onedrive_detector",
        "onedrive",
        "OneDrive",
        std::vector<std::string>{"onedrive"},
        std::vector<std::string>{"OneDrive", "OneDriveCommercial", "OneDriveConsumer"});
}

std::shared_ptr<IStorageProvider> make_dropbox_detector()
{
    return std::make_shared<CloudPathDetectorProvider>(
        "dropbox_detector",
        "dropbox",
        "Dropbox",
        std::vector<std::string>{"dropbox"});
}

std::shared_ptr<IStorageProvider> make_pcloud_detector()
{
    return std::make_shared<CloudPathDetectorProvider>(
        "pcloud_detector",
        "pcloud",
        "pCloud",
        std::vector<std::string>{"pcloud"});
}

std::shared_ptr<IStorageProvider> make_onedrive_provider()
{
    FileScannerBehavior behavior;
    behavior.skip_reparse_points = true;
    behavior.junk_name_prefixes = {"~$"};
    return std::make_shared<CloudCompatibilityProvider>(
        "onedrive",
        "OneDrive",
        std::vector<std::string>{"onedrive"},
        std::vector<std::string>{"OneDrive", "OneDriveCommercial", "OneDriveConsumer"},
        behavior);
}

std::shared_ptr<IStorageProvider> make_dropbox_provider()
{
    FileScannerBehavior behavior;
    behavior.skip_reparse_points = true;
    behavior.additional_junk_names = {".dropbox", ".dropbox.attr", ".dropbox.cache"};
    behavior.junk_name_prefixes = {"~$"};
    return std::make_shared<CloudCompatibilityProvider>(
        "dropbox",
        "Dropbox",
        std::vector<std::string>{"dropbox"},
        std::vector<std::string>{},
        behavior);
}

std::shared_ptr<IStorageProvider> make_pcloud_provider()
{
    FileScannerBehavior behavior;
    behavior.skip_reparse_points = true;
    behavior.additional_junk_names = {".pcloud"};
    behavior.junk_name_prefixes = {"~$"};
    return std::make_shared<CloudCompatibilityProvider>(
        "pcloud",
        "pCloud",
        std::vector<std::string>{"pcloud"},
        std::vector<std::string>{},
        behavior);
}

void append_cloud_storage_bundle(std::vector<std::shared_ptr<IStorageProvider>>& providers)
{
    providers.push_back(make_onedrive_provider());
    providers.push_back(make_dropbox_provider());
    providers.push_back(make_pcloud_provider());
}

} // namespace

const std::vector<StoragePluginManifest>& StoragePluginLoader::available_plugins() const
{
    return builtin_storage_plugin_manifests();
}

std::optional<StoragePluginManifest> StoragePluginLoader::find_plugin(const std::string& plugin_id) const
{
    return find_storage_plugin_manifest(plugin_id);
}

std::optional<StoragePluginManifest> StoragePluginLoader::find_plugin_for_provider(
    const std::string& provider_id) const
{
    return find_storage_plugin_manifest_for_provider(provider_id);
}

std::vector<std::shared_ptr<IStorageProvider>> StoragePluginLoader::create_detection_providers() const
{
    return {
        make_onedrive_detector(),
        make_dropbox_detector(),
        make_pcloud_detector()
    };
}

std::vector<std::shared_ptr<IStorageProvider>> StoragePluginLoader::create_providers_for_installed_plugins(
    const std::vector<std::string>& installed_plugin_ids) const
{
    std::vector<std::shared_ptr<IStorageProvider>> providers;
    for (const auto& plugin_id : installed_plugin_ids) {
        if (plugin_id == "cloud_storage_compat") {
            append_cloud_storage_bundle(providers);
        }
    }
    return providers;
}
