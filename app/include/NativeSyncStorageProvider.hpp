#pragma once

#include "OneDrivePlatform.hpp"
#include "StorageProvider.hpp"

/** @brief Implements storage operations for providers using the OS sync integration. */
class NativeSyncStorageProvider final : public IStorageProvider {
public:
    /**
     * @brief Constructs a provider using native sync metadata.
     * @param provider Sync provider whose roots may be detected.
     * @param requires_installation True for detection before a plugin is installed.
     */
    explicit NativeSyncStorageProvider(one_drive::SyncProvider provider, bool requires_installation = false);

    /**
     * @brief Constructs an installed provider or a detector for an uninstalled plugin.
     * @param provider Sync provider whose roots may be detected.
     * @param requires_installation True for detection before the plugin is installed.
     * @param platform Native filesystem probe. Tests can use a fixture probe.
     */
    NativeSyncStorageProvider(one_drive::SyncProvider provider, bool requires_installation,
                              const one_drive::PlatformProbe& platform);

    /**
     * @brief Returns the native Dropbox provider ID used by the manifest and undo records.
     * @return Stable ID distinct from the Dropbox compatibility provider.
     */
    static constexpr auto dropbox_provider_id() { return "dropbox_native"; }

    /** @copydoc IStorageProvider::id */
    std::string id() const override;
    /** @copydoc IStorageProvider::detect */
    StorageProviderDetection detect(const std::string& root_path) const override;
    /** @copydoc IStorageProvider::capabilities */
    StorageProviderCapabilities capabilities() const override;
    /** @copydoc IStorageProvider::list_directory */
    std::vector<FileEntry> list_directory(const std::string& directory, FileScanOptions options) const override;
    /** @copydoc IStorageProvider::inspect_path */
    StoragePathStatus inspect_path(const std::string& path) const override;
    /** @copydoc IStorageProvider::inspect_for_undo */
    StorageUndoCheck inspect_for_undo(const std::string& path, const std::string& expected_revision) const override;
    /** @copydoc IStorageProvider::preflight_move */
    StorageMovePreflight preflight_move(const std::string& source, const std::string& destination) const override;
    /** @copydoc IStorageProvider::path_exists */
    bool path_exists(const std::string& path) const override;
    /** @copydoc IStorageProvider::ensure_directory */
    bool ensure_directory(const std::string& path, std::string* error = nullptr) const override;
    /** @copydoc IStorageProvider::move_entry */
    StorageMutationResult move_entry(const std::string& source, const std::string& destination) const override;
    /** @copydoc IStorageProvider::undo_move */
    StorageMutationResult undo_move(const std::string& source, const std::string& destination) const override;
    /** @copydoc IStorageProvider::undo_move */
    StorageMutationResult undo_move(const std::string& source, const std::string& destination,
                                   const std::vector<std::string>& created_directories) const override;

private:
    StorageMutationResult rename_path(const std::filesystem::path& source,
                                      const std::filesystem::path& destination,
                                      const std::vector<std::string>& created_directories,
                                      bool capture_content) const;

    one_drive::SyncProvider provider_;
    bool requires_installation_;
    const one_drive::PlatformProbe& platform_;
};
