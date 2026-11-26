#pragma once

#include "StorageProvider.hpp"

#include <filesystem>
#include <functional>
#include <system_error>

/** @brief Moves local entries, including between filesystems, without replacing existing entries. */
class LocalFileTransfer {
public:
    using Path = std::filesystem::path;
    using TransferOperation = std::function<void(const Path&, const Path&, std::error_code&)>;
    using RemoveOperation = std::function<void(const Path&, std::error_code&)>;

    /** @brief Injectable filesystem operations; empty callbacks select production implementations. */
    struct Operations {
        TransferOperation rename;
        TransferOperation copy;
        RemoveOperation remove;
    };

    /**
     * @brief Constructs a transfer service with optional filesystem operation overrides.
     * @param operations Overrides used to exercise filesystem failures deterministically.
     */
    explicit LocalFileTransfer(Operations operations = {});

    /**
     * @brief Renames an entry or stages a complete copy before removing its source on EXDEV.
     * @param source Existing file, directory, or symlink to move.
     * @param destination Unoccupied destination whose parent directory already exists.
     * @return Outcome; a source-removal failure retains the completed destination copy.
     */
    StorageMutationResult move(const Path& source, const Path& destination) const;

private:
    /**
     * @brief Copies into destination-side staging, publishes exclusively, then removes the source.
     * @param source Entry on the source filesystem.
     * @param destination Unoccupied final destination.
     * @return Transfer outcome, including copy, publication, or removal diagnostics.
     */
    StorageMutationResult copy_and_remove(const Path& source, const Path& destination) const;

    Operations operations_;
};
