#include "StoragePluginLoader.hpp"

#include "CloudCompatibilityProvider.hpp"
#include "CloudPathDetectorProvider.hpp"
#include "ExternalProcessStorageProvider.hpp"
#include "NativeSyncStorageProvider.hpp"

#include <algorithm>
#include <unordered_map>

namespace {

std::shared_ptr<IStorageProvider> make_dropbox_detector()
{
    return std::make_shared<CloudPathDetectorProvider>(
        "dropbox_detector",
        "dropbox",
        "Dropbox",
        std::vector<std::string>{"dropbox"});
}

std::shared_ptr<IStorageProvider> make_onedrive_detector()
{
#if defined(__APPLE__) || defined(_WIN32)
    return std::make_shared<NativeSyncStorageProvider>(one_drive::SyncProvider::OneDrive, true);
#else
    return std::make_shared<CloudPathDetectorProvider>(
        "onedrive_detector",
        "onedrive",
        "OneDrive",
        std::vector<std::string>{"onedrive"});
#endif
}

#if defined(__APPLE__) || defined(_WIN32)
bool includes_native_dropbox(const StoragePluginManifest& manifest)
{
    const auto& providers = manifest.provider_ids;
    return std::find(providers.begin(), providers.end(), NativeSyncStorageProvider::dropbox_provider_id()) !=
           providers.end();
}

bool validate_synced_folder_bundle(const StoragePluginManifest& manifest, std::string* error)
{
    const auto& providers = manifest.provider_ids;
    const bool has_onedrive = std::find(providers.begin(), providers.end(), "onedrive") != providers.end();
    const bool valid_providers = has_onedrive &&
        (providers.size() == 1 || (providers.size() == 2 && includes_native_dropbox(manifest)));
    if (manifest.id == "onedrive_storage_support" && manifest.license_required &&
        manifest.product_id == manifest.id && valid_providers) {
        return true;
    }
    if (error) {
        *error = "Synced Folder Tools requires a supported provider list and matching licensed plugin and product IDs.";
    }
    return false;
}

StoragePluginEntryPointFactory::ProviderList make_synced_folder_bundle(const StoragePluginManifest& manifest)
{
    using one_drive::SyncProvider;
    StoragePluginEntryPointFactory::ProviderList providers;
    providers.push_back(std::make_shared<NativeSyncStorageProvider>(SyncProvider::OneDrive));
    if (includes_native_dropbox(manifest)) {
        providers.push_back(std::make_shared<NativeSyncStorageProvider>(SyncProvider::Dropbox));
    }
    return providers;
}
#endif

std::shared_ptr<IStorageProvider> make_pcloud_detector()
{
    return std::make_shared<CloudPathDetectorProvider>(
        "pcloud_detector",
        "pcloud",
        "pCloud",
        std::vector<std::string>{"pcloud"});
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
    providers.push_back(make_dropbox_provider());
    providers.push_back(make_pcloud_provider());
}

std::vector<std::shared_ptr<IStorageProvider>> make_cloud_detection_bundle(const StoragePluginManifest&)
{
    return {
        make_dropbox_detector(),
        make_pcloud_detector()
    };
}

std::vector<std::shared_ptr<IStorageProvider>> make_cloud_runtime_bundle(const StoragePluginManifest&)
{
    std::vector<std::shared_ptr<IStorageProvider>> providers;
    append_cloud_storage_bundle(providers);
    return providers;
}

std::vector<std::shared_ptr<IStorageProvider>> make_external_detection_bundle(
    const StoragePluginManifest& manifest)
{
    if (manifest.entry_point.empty()) {
        return {};
    }

    std::vector<std::shared_ptr<IStorageProvider>> providers;
    providers.reserve(manifest.provider_ids.size());
    for (const auto& provider_id : manifest.provider_ids) {
        providers.push_back(std::make_shared<ExternalProcessStorageProvider>(
            manifest,
            provider_id,
            provider_id + "_detector",
            true));
    }
    return providers;
}

std::vector<std::shared_ptr<IStorageProvider>> make_external_runtime_bundle(
    const StoragePluginManifest& manifest)
{
    if (manifest.entry_point.empty()) {
        return {};
    }

    std::vector<std::shared_ptr<IStorageProvider>> providers;
    providers.reserve(manifest.provider_ids.size());
    for (const auto& provider_id : manifest.provider_ids) {
        providers.push_back(std::make_shared<ExternalProcessStorageProvider>(
            manifest,
            provider_id,
            provider_id,
            false));
    }
    return providers;
}

void append_unique_providers(std::vector<std::shared_ptr<IStorageProvider>>& destination,
                             std::vector<std::shared_ptr<IStorageProvider>> source)
{
    for (auto& provider : source) {
        if (!provider) {
            continue;
        }

        const auto duplicate = std::find_if(
            destination.begin(),
            destination.end(),
            [&provider](const std::shared_ptr<IStorageProvider>& existing) {
                return existing && existing->id() == provider->id();
            });
        if (duplicate == destination.end()) {
            destination.push_back(std::move(provider));
        }
    }
}

std::vector<StoragePluginManifest> merge_manifests(const std::vector<StoragePluginManifest>& builtins,
                                                   std::vector<StoragePluginManifest> discovered)
{
    std::unordered_map<std::string, std::size_t> index_by_id;
    std::vector<StoragePluginManifest> merged;
    merged.reserve(builtins.size() + discovered.size());

    for (const auto& manifest : builtins) {
        index_by_id[manifest.id] = merged.size();
        merged.push_back(manifest);
    }

    for (auto& manifest : discovered) {
        const auto existing = index_by_id.find(manifest.id);
        if (existing != index_by_id.end()) {
            merged[existing->second] = std::move(manifest);
            continue;
        }

        index_by_id[manifest.id] = merged.size();
        merged.push_back(std::move(manifest));
    }

    return merged;
}

} // namespace

StoragePluginLoader::StoragePluginLoader(std::filesystem::path manifest_directory)
    : manifest_directory_(std::move(manifest_directory))
{
    entry_point_registry_.register_factory(
        "builtin_bundle",
        "cloud_storage_compat_bundle",
        StoragePluginEntryPointFactory{
            .validate_manifest = nullptr,
            .create_detection_providers = make_cloud_detection_bundle,
            .create_runtime_providers = make_cloud_runtime_bundle
        });
    entry_point_registry_.register_factory(
        "external_process",
        "*",
        StoragePluginEntryPointFactory{
            .validate_manifest = ExternalProcessStorageProvider::validate_plugin_manifest,
            .create_detection_providers = make_external_detection_bundle,
            .create_runtime_providers = make_external_runtime_bundle
        });
#if defined(__APPLE__) || defined(_WIN32)
    entry_point_registry_.register_factory(
        "builtin_bundle",
        "onedrive_storage_bundle",
        StoragePluginEntryPointFactory{
            .validate_manifest = validate_synced_folder_bundle,
            .create_runtime_providers = make_synced_folder_bundle
        });
#endif
}

std::vector<StoragePluginManifest> StoragePluginLoader::available_plugins() const
{
    if (manifest_directory_.empty()) {
        return builtin_storage_plugin_manifests();
    }

    std::string error;
    auto discovered = load_storage_plugin_manifests_from_directory(manifest_directory_, &error);
    (void)error;
    return merge_manifests(builtin_storage_plugin_manifests(), std::move(discovered));
}

std::optional<StoragePluginManifest> StoragePluginLoader::find_plugin(const std::string& plugin_id) const
{
    const auto manifests = available_plugins();
    for (const auto& manifest : manifests) {
        if (manifest.id == plugin_id) {
            return manifest;
        }
    }
    return std::nullopt;
}

std::optional<StoragePluginManifest> StoragePluginLoader::find_plugin_for_provider(
    const std::string& provider_id) const
{
    const auto manifests = available_plugins();
    for (const auto& manifest : manifests) {
        for (const auto& supported_provider_id : manifest.provider_ids) {
            if (supported_provider_id == provider_id) {
                return manifest;
            }
        }
    }
    return std::nullopt;
}

const std::filesystem::path& StoragePluginLoader::manifest_directory() const
{
    return manifest_directory_;
}

bool StoragePluginLoader::supports_plugin(const StoragePluginManifest& manifest,
                                         std::string* error) const
{
    return entry_point_registry_.supports(manifest, error);
}

std::vector<std::shared_ptr<IStorageProvider>> StoragePluginLoader::create_detection_providers() const
{
    std::vector<std::shared_ptr<IStorageProvider>> providers;
    for (const auto& manifest : available_plugins()) {
#if defined(__APPLE__) || defined(_WIN32)
        if (manifest.id == "onedrive_storage_support" && manifest.entry_point_kind == "external_process") {
            // The in-app detector also handles installations migrated from the executable package.
            continue;
        }
#endif
        append_unique_providers(providers, entry_point_registry_.create_detection_providers(manifest));
    }
    append_unique_providers(providers, {make_onedrive_detector()});
    return providers;
}

std::vector<std::shared_ptr<IStorageProvider>> StoragePluginLoader::create_providers_for_installed_plugins(
    const std::vector<StoragePluginManifest>& installed_plugins) const
{
    std::vector<std::shared_ptr<IStorageProvider>> providers;
    for (const auto& manifest : installed_plugins) {
        append_unique_providers(providers, entry_point_registry_.create_runtime_providers(manifest));
    }
    return providers;
}
