#pragma once

#include "StoragePluginManifest.hpp"
#include "StorageProvider.hpp"

#include <optional>
#include <string>

/**
 * @brief Proxies storage-provider operations to an external plugin process over stdio JSON.
 */
class ExternalProcessStorageProvider : public IStorageProvider {
public:
    ExternalProcessStorageProvider(StoragePluginManifest manifest,
                                   std::string provider_id,
                                   std::string instance_id,
                                   bool requires_installation);

    static bool validate_plugin_manifest(const StoragePluginManifest& manifest,
                                         std::string* error = nullptr);

    std::string id() const override;
    StorageProviderDetection detect(const std::string& root_path) const override;
    StorageProviderCapabilities capabilities() const override;
    std::vector<FileEntry> list_directory(const std::string& directory,
                                          FileScanOptions options) const override;
    StoragePathStatus inspect_path(const std::string& path) const override;
    StorageMovePreflight preflight_move(const std::string& source,
                                        const std::string& destination) const override;
    bool path_exists(const std::string& path) const override;
    bool ensure_directory(const std::string& directory, std::string* error = nullptr) const override;
    StorageMutationResult move_entry(const std::string& source,
                                     const std::string& destination) const override;
    StorageMutationResult undo_move(const std::string& source,
                                    const std::string& destination) const override;

private:
    std::optional<StorageProviderCapabilities> fetch_capabilities() const;

    StoragePluginManifest manifest_;
    std::string provider_id_;
    std::string instance_id_;
    bool requires_installation_{false};
    mutable std::optional<StorageProviderCapabilities> cached_capabilities_;
};
