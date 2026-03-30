#pragma once

#include "LocalFsProvider.hpp"

#include <string>
#include <vector>

/**
 * @brief Detects known cloud-synced folder paths and reports missing compatibility support.
 */
class CloudPathDetectorProvider : public IStorageProvider {
public:
    CloudPathDetectorProvider(std::string detector_id,
                              std::string logical_provider_id,
                              std::string display_name,
                              std::vector<std::string> path_markers,
                              std::vector<std::string> env_var_names = {});

    std::string id() const override;
    StorageProviderDetection detect(const std::string& root_path) const override;
    StorageProviderCapabilities capabilities() const override;
    std::vector<FileEntry> list_directory(const std::string& directory,
                                          FileScanOptions options) const override;
    bool path_exists(const std::string& path) const override;
    bool ensure_directory(const std::string& directory, std::string* error = nullptr) const override;
    StorageMutationResult move_entry(const std::string& source,
                                     const std::string& destination) const override;
    StorageMutationResult undo_move(const std::string& source,
                                    const std::string& destination) const override;

private:
    std::string detector_id_;
    std::string logical_provider_id_;
    std::string display_name_;
    std::vector<std::string> path_markers_;
    std::vector<std::string> env_var_names_;
    LocalFsProvider fallback_provider_;
};
