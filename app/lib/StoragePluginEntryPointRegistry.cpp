#include "StoragePluginEntryPointRegistry.hpp"

void StoragePluginEntryPointRegistry::register_factory(std::string entry_point_kind,
                                                       std::string entry_point,
                                                       StoragePluginEntryPointFactory factory)
{
    if (entry_point_kind.empty() || entry_point.empty()) {
        return;
    }

    factories_[key_for(entry_point_kind, entry_point)] = std::move(factory);
}

bool StoragePluginEntryPointRegistry::supports(const StoragePluginManifest& manifest,
                                              std::string* error) const
{
    const auto* factory = find_factory(manifest);
    if (!factory) {
        if (error) {
            *error = "No registered handler for this plugin entry point.";
        }
        return false;
    }
    if (!factory->validate_manifest) {
        return true;
    }
    return factory->validate_manifest(manifest, error);
}

StoragePluginEntryPointRegistry::ProviderList
StoragePluginEntryPointRegistry::create_detection_providers(const StoragePluginManifest& manifest) const
{
    const auto* factory = find_factory(manifest);
    if (!factory || !factory->create_detection_providers) {
        return {};
    }
    return factory->create_detection_providers(manifest);
}

StoragePluginEntryPointRegistry::ProviderList
StoragePluginEntryPointRegistry::create_runtime_providers(const StoragePluginManifest& manifest) const
{
    const auto* factory = find_factory(manifest);
    if (!factory || !factory->create_runtime_providers) {
        return {};
    }
    return factory->create_runtime_providers(manifest);
}

std::string StoragePluginEntryPointRegistry::key_for(const std::string& entry_point_kind,
                                                     const std::string& entry_point)
{
    return entry_point_kind + ":" + entry_point;
}

const StoragePluginEntryPointFactory* StoragePluginEntryPointRegistry::find_factory(
    const StoragePluginManifest& manifest) const
{
    auto it = factories_.find(key_for(manifest.entry_point_kind, manifest.entry_point));
    if (it != factories_.end()) {
        return &it->second;
    }

    it = factories_.find(key_for(manifest.entry_point_kind, "*"));
    if (it == factories_.end()) {
        return nullptr;
    }
    return &it->second;
}
