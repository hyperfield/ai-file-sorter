#pragma once

#include "FileScanner.hpp"
#include "LocalFsProvider.hpp"
#include "StorageProvider.hpp"

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Dedicated storage provider for OneDrive-backed folders.
 */
class OneDriveStorageProvider : public IStorageProvider {
public:
    OneDriveStorageProvider();

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
    StoragePathStatus inspect_local_path(const std::string& path) const;
    std::string make_revision_token(const std::filesystem::path& path,
                                    const StoragePathStatus& status) const;
    std::string make_stable_identity(const std::string& path) const;

    FileScannerBehavior scan_behavior_;
    FileScanner scanner_;
    LocalFsProvider fallback_provider_;
    std::vector<std::string> path_markers_;
    std::vector<std::string> env_var_names_;
};
