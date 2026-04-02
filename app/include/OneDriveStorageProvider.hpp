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
    /**
     * @brief Authoritative sync-root details returned by the operating system.
     */
    struct SyncRootInfo {
        std::string provider_name;
        std::string provider_version;
    };

    /**
     * @brief Remote OneDrive metadata returned by Graph-backed resolution.
     */
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

    /**
     * @brief Constructs a provider using the built-in sync-root and metadata resolvers.
     */
    OneDriveStorageProvider();
    /**
     * @brief Constructs a provider with a custom remote metadata resolver.
     * @param remote_metadata_resolver Resolver used to fetch Graph-backed OneDrive metadata.
     */
    explicit OneDriveStorageProvider(RemoteMetadataResolver remote_metadata_resolver);
    /**
     * @brief Constructs a provider with a custom sync-root resolver.
     * @param sync_root_resolver Resolver used to identify authoritative sync-root metadata.
     */
    explicit OneDriveStorageProvider(SyncRootResolver sync_root_resolver);
    /**
     * @brief Constructs a provider with custom remote metadata and sync-root resolvers.
     * @param remote_metadata_resolver Resolver used to fetch Graph-backed OneDrive metadata.
     * @param sync_root_resolver Resolver used to identify authoritative sync-root metadata.
     */
    OneDriveStorageProvider(RemoteMetadataResolver remote_metadata_resolver,
                            SyncRootResolver sync_root_resolver);

    /**
     * @brief Returns the OneDrive provider id.
     * @return The string literal `onedrive`.
     */
    std::string id() const override;
    /**
     * @brief Detects whether a selected root belongs to OneDrive.
     * @param root_path Candidate root path selected by the user.
     * @return Detection metadata preferring authoritative sync-root evidence over heuristics.
     */
    StorageProviderDetection detect(const std::string& root_path) const override;
    /**
     * @brief Returns behavioral flags for OneDrive-backed folders.
     * @return OneDrive capability flags consumed by scan, move, and undo code.
     */
    StorageProviderCapabilities capabilities() const override;
    /**
     * @brief Enumerates a OneDrive-backed directory with provider-aware scan behavior.
     * @param directory Directory path to enumerate.
     * @param options Active scan options controlling recursion and filters.
     * @return File entries visible to the app.
     */
    std::vector<FileEntry> list_directory(const std::string& directory,
                                          FileScanOptions options) const override;
    /**
     * @brief Inspects a path for OneDrive-specific status and metadata.
     * @param path Path to inspect.
     * @return OneDrive-aware path status and identity metadata.
     */
    StoragePathStatus inspect_path(const std::string& path) const override;
    /**
     * @brief Performs OneDrive-aware validation before a move.
     * @param source Source path about to be moved.
     * @param destination Destination path for the pending move.
     * @return Preflight result describing sync, hydration, and conflict conditions.
     */
    StorageMovePreflight preflight_move(const std::string& source,
                                        const std::string& destination) const override;
    /**
     * @brief Reports whether a path exists.
     * @param path Path to check.
     * @return True when the path exists on disk.
     */
    bool path_exists(const std::string& path) const override;
    /**
     * @brief Creates a directory tree required for a OneDrive-owned mutation.
     * @param directory Directory path to create.
     * @param error Optional output for a user-facing failure description.
     * @return True when the directory exists or was created successfully.
     */
    bool ensure_directory(const std::string& directory, std::string* error = nullptr) const override;
    /**
     * @brief Executes a OneDrive-owned move.
     * @param source Source path to move.
     * @param destination Destination path for the moved entry.
     * @return Mutation result including identity and revision metadata.
     */
    StorageMutationResult move_entry(const std::string& source,
                                     const std::string& destination) const override;
    /**
     * @brief Undoes a OneDrive-owned move.
     * @param source Current path of the moved entry.
     * @param destination Original destination to restore.
     * @return Mutation result describing the undo outcome.
     */
    StorageMutationResult undo_move(const std::string& source,
                                    const std::string& destination) const override;

private:
    /**
     * @brief Inspects provider-visible local filesystem state for a path.
     * @param path Path to inspect.
     * @return Local OneDrive-aware path status.
     */
    StoragePathStatus inspect_local_path(const std::string& path) const;
    /**
     * @brief Queries remote Graph-backed metadata when credentials are available.
     * @param path Local path to resolve.
     * @param error Optional output for resolver failures.
     * @return Remote metadata when Graph resolution succeeds.
     */
    std::optional<RemoteMetadata> query_remote_metadata(const std::string& path, std::string* error) const;
    /**
     * @brief Builds mutation metadata for a path from current provider status.
     * @param path Filesystem path to inspect.
     * @param status Provider status already gathered for the path.
     * @return Metadata persisted for undo validation.
     */
    StorageEntryMetadata build_metadata(const std::filesystem::path& path,
                                        const StoragePathStatus& status) const;
    /**
     * @brief Implements path existence using the underlying filesystem.
     * @param path Filesystem path to check.
     * @return True when the path exists locally.
     */
    bool path_exists_impl(const std::filesystem::path& path) const;
    /**
     * @brief Implements directory creation using the underlying filesystem.
     * @param directory Directory path to create.
     * @param error Optional output for a user-facing failure description.
     * @return True when the directory exists or was created successfully.
     */
    bool ensure_directory_impl(const std::filesystem::path& directory, std::string* error) const;
    /**
     * @brief Builds a revision token from local provider-visible metadata.
     * @param path Filesystem path being inspected.
     * @param status Provider status already gathered for the path.
     * @return Revision token used for undo validation.
     */
    std::string make_revision_token(const std::filesystem::path& path,
                                    const StoragePathStatus& status) const;
    /**
     * @brief Builds a stable identity token for a path.
     * @param path Local path string.
     * @return Provider identity token for rename/move tracking.
     */
    std::string make_stable_identity(const std::string& path) const;
    /**
     * @brief Retrieves authoritative sync-root details for a path.
     * @param path Local path to inspect.
     * @param error Optional output for resolver failures.
     * @return Sync-root metadata when authoritative detection succeeds.
     */
    std::optional<SyncRootInfo> query_sync_root_info(const std::string& path, std::string* error) const;
    /**
     * @brief Resolves sync-root details using the built-in platform integration.
     * @param path Local path to inspect.
     * @param error Optional output for resolver failures.
     * @return Sync-root metadata when authoritative detection succeeds.
     */
    static std::optional<SyncRootInfo> resolve_sync_root_info(const std::string& path, std::string* error);
    /**
     * @brief Resolves remote Graph metadata using configured environment-based credentials.
     * @param path Local path to inspect.
     * @param env_var_names Environment variables describing the OneDrive root.
     * @param error Optional output for resolver failures.
     * @return Graph metadata when remote resolution succeeds.
     */
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
