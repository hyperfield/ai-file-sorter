#pragma once

#include "Types.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

/**
 * @brief Result of asking a provider whether it should handle a given path.
 */
struct StorageProviderDetection {
    std::string provider_id;
    bool matched{false};
    bool needs_additional_support{false};
    int confidence{0};
    std::string detection_source;
    std::string message;
};

/**
 * @brief Declares behavior differences the app may need to account for.
 */
struct StorageProviderCapabilities {
    bool supports_online_only_files{false};
    bool supports_atomic_rename{true};
    bool should_skip_reparse_points{false};
    bool should_relax_undo_mtime_validation{false};
};

/**
 * @brief Describes provider-observed state for a path.
 */
struct StoragePathStatus {
    bool exists{false};
    bool hydration_required{false};
    bool sync_locked{false};
    bool conflict_copy{false};
    bool should_retry{false};
    int retry_after_ms{0};
    std::string stable_identity;
    std::string revision_token;
    std::string message;
};

/**
 * @brief Describes whether a pending move should proceed.
 */
struct StorageMovePreflight {
    bool allowed{true};
    bool skipped{false};
    bool hydration_required{false};
    bool sync_locked{false};
    bool destination_conflict{false};
    bool should_retry{false};
    int retry_after_ms{0};
    StoragePathStatus source_status;
    StoragePathStatus destination_status;
    std::string message;
};

/**
 * @brief Metadata captured after a successful storage move.
 */
struct StorageEntryMetadata {
    std::uintmax_t size_bytes{0};
    std::time_t mtime{0};
    std::string stable_identity;
    std::string revision_token;
};

/**
 * @brief Result of a move or undo operation handled by a storage provider.
 */
struct StorageMutationResult {
    bool success{false};
    bool skipped{false};
    std::string message;
    StorageEntryMetadata metadata;
};

/**
 * @brief Abstracts directory enumeration behind a storage-provider contract.
 */
class IStorageProvider {
public:
    virtual ~IStorageProvider() = default;

    /**
     * @brief Returns the stable provider identifier.
     * @return Provider id used for registry lookup and persistence.
     */
    virtual std::string id() const = 0;
    /**
     * @brief Detects whether the provider should handle a given root path.
     * @param root_path Candidate root path selected by the user.
     * @return Detection metadata including confidence and user-facing diagnostics.
     */
    virtual StorageProviderDetection detect(const std::string& root_path) const = 0;
    /**
     * @brief Describes capabilities and behavioral differences for the provider.
     * @return Capability flags consumed by the app and undo logic.
     */
    virtual StorageProviderCapabilities capabilities() const = 0;
    /**
     * @brief Enumerates files and directories under a root path.
     * @param directory Directory path to enumerate.
     * @param options Active scan options controlling recursion and filters.
     * @return Discovered file entries for analysis and review.
     */
    virtual std::vector<FileEntry> list_directory(const std::string& directory,
                                                  FileScanOptions options) const = 0;
    /**
     * @brief Inspects provider-visible status for a path.
     * @param path Path to inspect.
     * @return Provider status including hydration, sync, identity, and revision metadata.
     */
    virtual StoragePathStatus inspect_path(const std::string& path) const = 0;
    /**
     * @brief Validates whether a move operation should proceed.
     * @param source Source path about to be moved.
     * @param destination Destination path for the pending move.
     * @return Preflight result describing whether the move is safe or should be retried/skipped.
     */
    virtual StorageMovePreflight preflight_move(const std::string& source,
                                                const std::string& destination) const = 0;
    /**
     * @brief Checks whether a path exists according to the provider.
     * @param path Path to check.
     * @return True when the provider reports the path as existing.
     */
    virtual bool path_exists(const std::string& path) const = 0;
    /**
     * @brief Creates a directory tree required for a provider-owned mutation.
     * @param directory Directory path to create.
     * @param error Optional output for a user-facing failure description.
     * @return True when the directory exists or was created successfully.
     */
    virtual bool ensure_directory(const std::string& directory, std::string* error = nullptr) const = 0;
    /**
     * @brief Executes a provider-owned move operation.
     * @param source Source path to move.
     * @param destination Destination path for the moved entry.
     * @return Mutation result including provider metadata for undo.
     */
    virtual StorageMutationResult move_entry(const std::string& source,
                                             const std::string& destination) const = 0;
    /**
     * @brief Reverses a previously recorded provider-owned move.
     * @param source Current path of the moved entry.
     * @param destination Original destination to restore.
     * @return Mutation result describing the undo outcome.
     */
    virtual StorageMutationResult undo_move(const std::string& source,
                                            const std::string& destination) const = 0;
};
