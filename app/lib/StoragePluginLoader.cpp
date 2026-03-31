#include "StoragePluginLoader.hpp"

#include "CloudCompatibilityProvider.hpp"
#include "CloudPathDetectorProvider.hpp"
#include "ExternalProcessStorageProvider.hpp"

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
        append_unique_providers(providers, entry_point_registry_.create_detection_providers(manifest));
    }
    return providers;
}

std::vector<std::shared_ptr<IStorageProvider>> StoragePluginLoader::create_providers_for_installed_plugins(
    const std::vector<std::string>& installed_plugin_ids) const
{
    std::vector<std::shared_ptr<IStorageProvider>> providers;
    for (const auto& plugin_id : installed_plugin_ids) {
        const auto manifest = find_plugin(plugin_id);
        if (!manifest.has_value()) {
            continue;
        }
        append_unique_providers(providers, entry_point_registry_.create_runtime_providers(*manifest));
    }
    return providers;
}
