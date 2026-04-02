#pragma once

#include "FileScanner.hpp"
#include "StorageProvider.hpp"

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Dedicated storage provider for OneDrive-backed folders.
 */
class OneDriveStorageProvider : public IStorageProvider {
public:
    struct SyncRootInfo {
        std::string provider_name;
        std::string provider_version;
    };

    struct RemoteMetadata {
        std::string drive_id;
        std::string item_id;
        std::string e_tag;
        std::string c_tag;
    };

    using RemoteMetadataResolver =
        std::function<std::optional<RemoteMetadata>(const std::string& path, std::string* error)>;
    using SyncRootResolver =
        std::function<std::optional<SyncRootInfo>(const std::string& path, std::string* error)>;

    OneDriveStorageProvider();
    explicit OneDriveStorageProvider(RemoteMetadataResolver remote_metadata_resolver);
    explicit OneDriveStorageProvider(SyncRootResolver sync_root_resolver);
    OneDriveStorageProvider(RemoteMetadataResolver remote_metadata_resolver,
                            SyncRootResolver sync_root_resolver);

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
    std::optional<RemoteMetadata> query_remote_metadata(const std::string& path, std::string* error) const;
    StorageEntryMetadata build_metadata(const std::filesystem::path& path,
                                        const StoragePathStatus& status) const;
    bool path_exists_impl(const std::filesystem::path& path) const;
    bool ensure_directory_impl(const std::filesystem::path& directory, std::string* error) const;
    std::string make_revision_token(const std::filesystem::path& path,
                                    const StoragePathStatus& status) const;
    std::string make_stable_identity(const std::string& path) const;
    std::optional<SyncRootInfo> query_sync_root_info(const std::string& path, std::string* error) const;
    static std::optional<SyncRootInfo> resolve_sync_root_info(const std::string& path, std::string* error);
    static std::optional<RemoteMetadata> resolve_graph_metadata(const std::string& path,
                                                                const std::vector<std::string>& env_var_names,
                                                                std::string* error);

    FileScannerBehavior scan_behavior_;
    FileScanner scanner_;
    std::vector<std::string> path_markers_;
    std::vector<std::string> env_var_names_;
    RemoteMetadataResolver remote_metadata_resolver_;
    SyncRootResolver sync_root_resolver_;
    mutable std::unordered_map<std::string, std::optional<SyncRootInfo>> sync_root_info_cache_;
};
