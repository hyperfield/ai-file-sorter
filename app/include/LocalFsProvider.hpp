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
    StoragePathStatus inspect_path(const std::string& path) const override;
    StorageMovePreflight preflight_move(const std::string& source,
                                        const std::string& destination) const override;
    bool path_exists(const std::string& path) const override;
    bool ensure_directory(const std::string& directory, std::string* error = nullptr) const override;
    StorageMutationResult move_entry(const std::string& source,
                                     const std::string& destination) const override;
    StorageMutationResult undo_move(const std::string& source,
                                    const std::string& destination) const override;
    /**
     * @brief Restores a move and removes only directories recorded as app-created.
     * @param source Original path to restore.
     * @param destination Current path of the moved entry.
     * @param created_directories Directories the app created for the move.
     * @return Mutation result describing the undo outcome.
     */
    StorageMutationResult undo_move(const std::string& source,
                                    const std::string& destination,
                                    const std::vector<std::string>& created_directories) const override;

private:
    /**
     * @brief Restores a moved local filesystem entry without directory cleanup.
     * @param source Original path to restore.
     * @param destination Current path of the moved entry.
     * @return Mutation result describing the restore outcome.
     */
    StorageMutationResult restore_moved_entry(const std::string& source,
                                              const std::string& destination) const;

    FileScanner scanner_;
};
