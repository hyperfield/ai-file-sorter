#pragma once

#include "StoragePluginManifest.hpp"
#include "StorageProvider.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Creates storage providers for a manifest-declared plugin entry point.
 */
struct StoragePluginEntryPointFactory {
    using ProviderList = std::vector<std::shared_ptr<IStorageProvider>>;

    std::function<bool(const StoragePluginManifest&, std::string*)> validate_manifest;
    std::function<ProviderList(const StoragePluginManifest&)> create_detection_providers;
    std::function<ProviderList(const StoragePluginManifest&)> create_runtime_providers;
};

/**
 * @brief Maps manifest entry points to provider factory implementations.
 */
class StoragePluginEntryPointRegistry {
public:
    using ProviderList = std::vector<std::shared_ptr<IStorageProvider>>;

    void register_factory(std::string entry_point_kind,
                          std::string entry_point,
                          StoragePluginEntryPointFactory factory);

    bool supports(const StoragePluginManifest& manifest, std::string* error = nullptr) const;
    ProviderList create_detection_providers(const StoragePluginManifest& manifest) const;
    ProviderList create_runtime_providers(const StoragePluginManifest& manifest) const;

private:
    static std::string key_for(const std::string& entry_point_kind,
                               const std::string& entry_point);
    const StoragePluginEntryPointFactory* find_factory(const StoragePluginManifest& manifest) const;

    std::unordered_map<std::string, StoragePluginEntryPointFactory> factories_;
};
