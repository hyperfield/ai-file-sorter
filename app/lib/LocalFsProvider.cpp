#include "LocalFsProvider.hpp"

#include "Utils.hpp"

#include <chrono>
#include <filesystem>

namespace {

StorageEntryMetadata read_metadata(const std::filesystem::path& path)
{
    StorageEntryMetadata metadata;
    std::error_code ec;
    metadata.size_bytes = std::filesystem::file_size(path, ec);
    if (ec) {
        metadata.size_bytes = 0;
        ec.clear();
    }

    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        const auto delta = write_time - std::filesystem::file_time_type::clock::now();
        const auto system_time = std::chrono::system_clock::now() +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(delta);
        metadata.mtime = std::chrono::system_clock::to_time_t(system_time);
    }

    metadata.stable_identity = Utils::path_to_utf8(path.lexically_normal());
    metadata.revision_token =
        std::to_string(metadata.size_bytes) + ":" + std::to_string(metadata.mtime);

    return metadata;
}

void remove_empty_parent_directories(const std::filesystem::path& moved_from)
{
    std::error_code ec;
    auto parent = moved_from.parent_path();
    while (!parent.empty()) {
        if (!std::filesystem::exists(parent, ec) || ec) {
            break;
        }
        if (std::filesystem::is_directory(parent, ec) &&
            std::filesystem::is_empty(parent, ec) && !ec) {
            std::filesystem::remove(parent, ec);
            parent = parent.parent_path();
            continue;
        }
        break;
    }
}

} // namespace

std::string LocalFsProvider::id() const
{
    return "local_fs";
}

StorageProviderDetection LocalFsProvider::detect(const std::string& root_path) const
{
    (void)root_path;
    return StorageProviderDetection{
        .provider_id = id(),
        .matched = true,
        .needs_additional_support = false,
        .confidence = 1,
        .detection_source = "default_fallback",
        .message = {}
    };
}

StorageProviderCapabilities LocalFsProvider::capabilities() const
{
    return StorageProviderCapabilities{
        .supports_online_only_files = false,
        .supports_atomic_rename = true,
        .should_skip_reparse_points = false,
        .should_relax_undo_mtime_validation = false
    };
}

std::vector<FileEntry> LocalFsProvider::list_directory(const std::string& directory,
                                                       FileScanOptions options) const
{
    return scanner_.get_directory_entries(directory, options, {});
}

StoragePathStatus LocalFsProvider::inspect_path(const std::string& path) const
{
    StoragePathStatus status;
    status.exists = path_exists(path);
    status.stable_identity = path;
    if (!status.exists) {
        status.message = "Path does not exist.";
        return status;
    }

    const auto metadata = read_metadata(Utils::utf8_to_path(path));
    status.stable_identity = metadata.stable_identity;
    status.revision_token = metadata.revision_token;
    return status;
}

StorageMovePreflight LocalFsProvider::preflight_move(const std::string& source,
                                                     const std::string& destination) const
{
    StorageMovePreflight preflight;
    preflight.source_status = inspect_path(source);
    preflight.destination_status = inspect_path(destination);

    if (!preflight.source_status.exists) {
        preflight.allowed = false;
        preflight.skipped = true;
        preflight.message = "Source path is missing.";
        return preflight;
    }

    if (preflight.destination_status.exists) {
        preflight.allowed = false;
        preflight.skipped = true;
        preflight.destination_conflict = true;
        preflight.message = "Destination path already exists.";
        return preflight;
    }

    return preflight;
}

bool LocalFsProvider::path_exists(const std::string& path) const
{
    std::error_code ec;
    return std::filesystem::exists(Utils::utf8_to_path(path), ec);
}

bool LocalFsProvider::ensure_directory(const std::string& directory, std::string* error) const
{
    if (directory.empty()) {
        if (error) {
            *error = "Directory path is empty.";
        }
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(Utils::utf8_to_path(directory), ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    return true;
}

StorageMutationResult LocalFsProvider::move_entry(const std::string& source,
                                                  const std::string& destination) const
{
    StorageMutationResult result;
    const auto source_path = Utils::utf8_to_path(source);
    const auto destination_path = Utils::utf8_to_path(destination);

    const auto preflight = preflight_move(source, destination);
    if (!preflight.allowed) {
        result.skipped = preflight.skipped;
        result.message = preflight.message;
        return result;
    }

    std::string ensure_error;
    if (!ensure_directory(Utils::path_to_utf8(destination_path.parent_path()), &ensure_error)) {
        result.message = ensure_error.empty() ? "Failed to create destination directories." : ensure_error;
        return result;
    }

    std::error_code ec;
    std::filesystem::rename(source_path, destination_path, ec);
    if (ec) {
        result.message = ec.message();
        return result;
    }

    result.success = true;
    result.metadata = read_metadata(destination_path);
    return result;
}

StorageMutationResult LocalFsProvider::undo_move(const std::string& source,
                                                 const std::string& destination) const
{
    StorageMutationResult result;
    const auto source_path = Utils::utf8_to_path(source);
    const auto destination_path = Utils::utf8_to_path(destination);

    const auto source_status = inspect_path(source);
    const auto destination_status = inspect_path(destination);
    if (!destination_status.exists) {
        result.skipped = true;
        result.message = "Destination path is missing.";
        return result;
    }

    if (source_status.exists) {
        result.skipped = true;
        result.message = "Source path already exists.";
        return result;
    }

    std::string ensure_error;
    if (!ensure_directory(Utils::path_to_utf8(source_path.parent_path()), &ensure_error)) {
        result.message = ensure_error.empty() ? "Failed to create source directories." : ensure_error;
        return result;
    }

    std::error_code ec;
    std::filesystem::rename(destination_path, source_path, ec);
    if (ec) {
        result.message = ec.message();
        return result;
    }

    remove_empty_parent_directories(destination_path);
    result.success = true;
    return result;
}
