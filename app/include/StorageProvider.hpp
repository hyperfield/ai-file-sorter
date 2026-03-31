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

    virtual std::string id() const = 0;
    virtual StorageProviderDetection detect(const std::string& root_path) const = 0;
    virtual StorageProviderCapabilities capabilities() const = 0;
    virtual std::vector<FileEntry> list_directory(const std::string& directory,
                                                  FileScanOptions options) const = 0;
    virtual StoragePathStatus inspect_path(const std::string& path) const = 0;
    virtual StorageMovePreflight preflight_move(const std::string& source,
                                                const std::string& destination) const = 0;
    virtual bool path_exists(const std::string& path) const = 0;
    virtual bool ensure_directory(const std::string& directory, std::string* error = nullptr) const = 0;
    virtual StorageMutationResult move_entry(const std::string& source,
                                             const std::string& destination) const = 0;
    virtual StorageMutationResult undo_move(const std::string& source,
                                            const std::string& destination) const = 0;
};
