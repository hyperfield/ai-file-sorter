#pragma once

#include "FileScanner.hpp"
#include "LocalFsProvider.hpp"
#include "StorageProvider.hpp"

#include <string>
#include <vector>

/**
 * @brief Storage provider for cloud-synced folders with safer scan and undo defaults.
 */
class CloudCompatibilityProvider : public IStorageProvider {
public:
    CloudCompatibilityProvider(std::string provider_id,
                               std::string display_name,
                               std::vector<std::string> path_markers,
                               std::vector<std::string> env_var_names = {},
                               FileScannerBehavior scan_behavior = {},
                               StorageProviderCapabilities capabilities = {});

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
    std::string provider_id_;
    std::string display_name_;
    std::vector<std::string> path_markers_;
    std::vector<std::string> env_var_names_;
    StorageProviderCapabilities capabilities_;
    FileScannerBehavior scan_behavior_;
    FileScanner scanner_;
    LocalFsProvider fallback_provider_;
};
