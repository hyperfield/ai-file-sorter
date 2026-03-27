#pragma once

#include "FileScanner.hpp"
#include "StorageProvider.hpp"

/**
 * @brief Default provider for normal local filesystems and mounted shares.
 */
class LocalFsProvider : public IStorageProvider {
public:
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
    FileScanner scanner_;
};
