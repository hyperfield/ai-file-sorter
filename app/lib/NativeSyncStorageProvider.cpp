#include "NativeSyncStorageProvider.hpp"

#include "Logger.hpp"
#include "ProtectedProjectDetector.hpp"

#ifdef __APPLE__
#include "OneDriveMacMutations.hpp"
#endif

#include <QFileInfo>

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

namespace fs = std::filesystem;
using one_drive::PathInspection;
using one_drive::PlatformFileInfo;
using one_drive::PlatformProbe;
using one_drive::SyncProvider;
using one_drive::inspect_sync_path;
using one_drive::path_from_qstring;
using one_drive::path_to_qstring;

constexpr std::string_view kOneDrivePrefix = "onedrive:";
constexpr int kInstalledProviderConfidenceBonus = 20;

const char* provider_id(SyncProvider provider)
{
    return provider == SyncProvider::Dropbox ? NativeSyncStorageProvider::dropbox_provider_id() : "onedrive";
}

std::string_view token_prefix(SyncProvider provider)
{
    return provider == SyncProvider::Dropbox ? "dropbox:" : kOneDrivePrefix;
}

std::string qualified_native_token(const QString& value, SyncProvider provider)
{
    auto token = value.toStdString();
    // Preserve the existing OneDrive record format used by the platform primitives.
    if (provider == SyncProvider::Dropbox && token.starts_with(kOneDrivePrefix)) {
        token.replace(0, kOneDrivePrefix.size(), token_prefix(provider));
    }
    return token;
}

fs::path native_path(const std::string& path)
{
    if (path.find('\0') != std::string::npos) {
        return {};
    }
    return path_from_qstring(QString::fromStdString(path));
}

bool is_protected_project(const ProtectedProjectDetector& detector, const fs::path& path)
{
    const auto project = detector.detect(path);
    if (!project || !ProtectedProjectDetector::should_skip(*project)) {
        return false;
    }
    if (const auto logger = Logger::get_logger("core_logger")) {
        logger->info("Skipping protected {} at '{}': {}", project->name,
                     path_to_qstring(project->root).toStdString(), project->reason);
    }
    return true;
}

bool append_scan_entry(std::vector<FileEntry>& entries, const fs::path& path,
                       FileScanOptions options, const ProtectedProjectDetector& detector, SyncProvider provider)
{
    if (provider == SyncProvider::Dropbox && one_drive::is_dropbox_metadata_name(path_to_qstring(path.filename()))) {
        return false;
    }
    const auto info = one_drive::scan_entry_info(path);
    if (info.hidden && !has_flag(options, FileScanOptions::HiddenFiles)) {
        return false;
    }
    if (info.directory && is_protected_project(detector, path)) {
        return false;
    }
    const bool include = info.directory ? has_flag(options, FileScanOptions::Directories)
        : info.regular_file && has_flag(options, FileScanOptions::Files);
    if (include) {
        const auto text = path_to_qstring(path);
#ifdef __APPLE__
        if (path_from_qstring(text) != path) {
            throw fs::filesystem_error("Cannot encode filename as UTF-8", path,
                                       std::make_error_code(std::errc::illegal_byte_sequence));
        }
#endif
        entries.push_back({
            .full_path = text.toStdString(),
            .file_name = path_to_qstring(path.filename()).toStdString(),
            .type = info.directory ? FileType::Directory : FileType::File
        });
    }
    return info.directory;
}

std::string revision_token(const PlatformFileInfo& platform, SyncProvider provider)
{
    if (platform.revision_suffix.isEmpty()) {
        return {};
    }
    return std::string(token_prefix(provider)) + platform.revision_suffix.toStdString();
}

StoragePathStatus make_status(const PathInspection& inspection, SyncProvider provider)
{
    const auto& state = inspection.state;
    return {
        .exists = inspection.exists,
        .hydration_required = state.hydration_required,
        .sync_locked = state.sync_locked,
        .conflict_copy = state.conflict_copy,
        .should_retry = state.should_retry,
        .retry_after_ms = state.retry_after_ms,
        .stable_identity = qualified_native_token(state.stable_identity, provider),
        .revision_token = inspection.exists ? revision_token(state, provider) : std::string{},
        .message = state.message.toStdString()
    };
}

StorageMovePreflight preflight(const fs::path& source, const fs::path& destination,
                               const PlatformProbe& platform, SyncProvider provider)
{
    const auto source_info = inspect_sync_path(source, platform, provider);
    const auto destination_info = inspect_sync_path(destination, platform, provider);
    const auto* state = &source_info.state;
    std::string message;
    if (source_info.state.move_blocked) {
        message = source_info.state.message.toStdString();
    } else if (!source_info.exists) {
        message = "Source path is missing.";
    } else if (destination_info.exists) {
        message = "Destination path already exists.";
    } else if (destination_info.state.move_blocked) {
        state = &destination_info.state;
        message = state->message.toStdString();
    } else if (source_info.state.sync_locked) {
        message = state->message.toStdString();
    }

    const bool allowed = source_info.exists && !destination_info.exists && !source_info.state.sync_locked &&
                         !source_info.state.move_blocked && !destination_info.state.move_blocked;
    return {
        .allowed = allowed,
        .skipped = !allowed,
        .hydration_required = state->hydration_required,
        .sync_locked = state->sync_locked,
        .destination_conflict = destination_info.exists,
        .should_retry = state->should_retry,
        .retry_after_ms = state->retry_after_ms,
        .source_status = make_status(source_info, provider),
        .destination_status = make_status(destination_info, provider),
        .message = std::move(message)
    };
}

StorageEntryMetadata moved_metadata(const fs::path& path, const PathInspection& inspection,
                                   bool capture_content, SyncProvider provider)
{
    const auto& state = inspection.state;
    auto revision = revision_token(state, provider);
#ifdef _WIN32
    const bool can_capture = state.stable_identity.startsWith(QStringLiteral("onedrive:fileid:")) &&
        state.revision_suffix.startsWith(QStringLiteral("write-v1:")) && QFileInfo(path_to_qstring(path)).isFile();
#else
    const bool can_capture = state.can_capture_content_revision;
#endif
    if (capture_content && can_capture && !state.hydration_required) {
        const auto snapshot = one_drive::local_content_revision(path, state.stable_identity, state.revision_suffix);
        if (!snapshot.isEmpty()) {
            revision = qualified_native_token(snapshot, provider);
        }
    }
    return {
        .size_bytes = static_cast<std::uintmax_t>(inspection.size_bytes),
        .mtime = static_cast<std::time_t>(inspection.mtime),
        .stable_identity = qualified_native_token(state.stable_identity, provider),
        .revision_token = std::move(revision)
    };
}

#ifndef __APPLE__
void remove_empty_directories(const std::vector<std::string>& directories)
{
    for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
        const auto path = native_path(*it);
        std::error_code error;
        if (fs::is_directory(path, error) && !error && fs::is_empty(path, error) && !error) {
            fs::remove(path, error);
        }
    }
}
#endif

} // namespace

NativeSyncStorageProvider::NativeSyncStorageProvider(SyncProvider provider, bool requires_installation)
    : NativeSyncStorageProvider(provider, requires_installation, one_drive::default_platform_probe(provider))
{
}

NativeSyncStorageProvider::NativeSyncStorageProvider(
    SyncProvider provider, bool requires_installation, const PlatformProbe& platform)
    : provider_(provider), requires_installation_(requires_installation), platform_(platform)
{
}

std::string NativeSyncStorageProvider::id() const
{
    const std::string id = provider_id(provider_);
    return requires_installation_ ? id + "_detector" : id;
}

StorageProviderDetection NativeSyncStorageProvider::detect(const std::string& root_path) const
{
    const auto root = native_path(root_path);
    if (root.empty()) {
        return {};
    }
    const bool dropbox = provider_ == SyncProvider::Dropbox;
    const auto detection = dropbox ? platform_.detect_sync_root(root) : one_drive::detect_onedrive_root(root, platform_);
    if (!detection.matched || detection.vetoed || (dropbox && !detection.authoritative)) {
        return {};
    }
    return {
        .provider_id = provider_id(provider_),
        .matched = true,
        .needs_additional_support = requires_installation_,
        .confidence = detection.confidence + (requires_installation_ ? 0 : kInstalledProviderConfidenceBonus),
        .detection_source = detection.source.toStdString(),
        .message = detection.message.toStdString()
    };
}

StorageProviderCapabilities NativeSyncStorageProvider::capabilities() const
{
    return {
        .supports_online_only_files = true,
        .supports_atomic_rename = false,
        .should_skip_reparse_points = true
    };
}

std::vector<FileEntry> NativeSyncStorageProvider::list_directory(
    const std::string& directory, FileScanOptions options) const
{
    auto root = native_path(directory);
    if (root.empty()) {
        throw std::invalid_argument("Invalid synced directory path.");
    }
#ifdef __APPLE__
    // Keep the selected root spelling even if the directory is reached through a symlink.
    root = fs::absolute(root);
#endif
    std::vector<FileEntry> entries;
    const ProtectedProjectDetector detector;
    if ((provider_ == SyncProvider::Dropbox && one_drive::is_dropbox_metadata_path(root)) ||
        is_protected_project(detector, root)) {
        return entries;
    }
    if (has_flag(options, FileScanOptions::Recursive)) {
        for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
            if (!append_scan_entry(entries, it->path(), options, detector, provider_)) {
                it.disable_recursion_pending();
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(root)) {
            append_scan_entry(entries, entry.path(), options, detector, provider_);
        }
    }
    return entries;
}

StoragePathStatus NativeSyncStorageProvider::inspect_path(const std::string& path) const
{
    return make_status(inspect_sync_path(native_path(path), platform_, provider_), provider_);
}

StorageUndoCheck NativeSyncStorageProvider::inspect_for_undo(
    const std::string& path, const std::string& expected_revision) const
{
    const auto native = native_path(path);
    StorageUndoCheck result{make_status(inspect_sync_path(native, platform_, provider_), provider_), std::nullopt};
    const auto content_prefix = std::string(token_prefix(provider_)) + "sha256:";
    if (!expected_revision.starts_with(content_prefix)) {
        return result;
    }
    const auto revision = one_drive::local_content_revision(native, {}, {});
    result.content_matches = qualified_native_token(revision, provider_) == expected_revision;
    const auto name = provider_ == SyncProvider::Dropbox ? "Dropbox" : "OneDrive";
    if (revision.isEmpty()) {
        result.status.message = QStringLiteral("Could not verify %1 contents. Make sure the file is available locally and retry.")
                                    .arg(QString::fromLatin1(name)).toStdString();
    } else if (!*result.content_matches) {
        result.status.message = QStringLiteral("%1 file contents changed since the move.")
                                    .arg(QString::fromLatin1(name)).toStdString();
    }
    return result;
}

StorageMovePreflight NativeSyncStorageProvider::preflight_move(
    const std::string& source, const std::string& destination) const
{
    return preflight(native_path(source), native_path(destination), platform_, provider_);
}

bool NativeSyncStorageProvider::path_exists(const std::string& path) const
{
    const auto native = native_path(path);
#ifdef __APPLE__
    return platform_.inspect_path(native).exists;
#else
    std::error_code error;
    return fs::exists(native, error) && !error;
#endif
}

bool NativeSyncStorageProvider::ensure_directory(const std::string& path, std::string* error) const
{
    const auto native = native_path(path);
    if (provider_ == SyncProvider::Dropbox) {
        const auto inspection = inspect_sync_path(native, platform_, provider_);
        if (inspection.state.move_blocked) {
            if (error) {
                *error = inspection.state.message.toStdString();
            }
            return false;
        }
    }
#ifdef __APPLE__
    const auto result = one_drive::ensure_macos_directory(native);
    if (!result.error_code.isEmpty()) {
        if (error) {
            *error = result.message.toStdString();
        }
        return false;
    }
#else
    std::error_code filesystem_error;
    fs::create_directories(native, filesystem_error);
    if (filesystem_error) {
        if (error) {
            *error = filesystem_error.message();
        }
        return false;
    }
#endif
    return true;
}

StorageMutationResult NativeSyncStorageProvider::move_entry(const std::string& source, const std::string& destination) const
{
    return rename_path(native_path(source), native_path(destination), {}, true);
}

StorageMutationResult NativeSyncStorageProvider::undo_move(const std::string& source, const std::string& destination) const
{
    return undo_move(source, destination, {});
}

StorageMutationResult NativeSyncStorageProvider::undo_move(
    const std::string& source, const std::string& destination, const std::vector<std::string>& created_directories) const
{
    return rename_path(native_path(destination), native_path(source), created_directories, false);
}

StorageMutationResult NativeSyncStorageProvider::rename_path(
    const fs::path& source, const fs::path& destination, const std::vector<std::string>& created_directories,
    bool capture_content) const
{
    const auto check = preflight(source, destination, platform_, provider_);
    if (!check.allowed) {
        return {.skipped = check.skipped, .message = check.message};
    }
#ifdef __APPLE__
    std::vector<fs::path> paths;
    paths.reserve(created_directories.size());
    for (const auto& path : created_directories) {
        paths.push_back(native_path(path));
    }
    const auto result = one_drive::move_macos_path(source, destination, paths);
    if (!result.moved_metadata) {
        return {.skipped = result.skipped, .message = result.message.toStdString()};
    }
    return {.success = true, .metadata = moved_metadata(destination, *result.moved_metadata, capture_content, provider_)};
#else
    std::error_code error;
    if (const auto parent = destination.parent_path(); !parent.empty()) {
        fs::create_directories(parent, error);
        if (error) {
            return {.message = error.message()};
        }
    }
#ifdef _WIN32
    if (provider_ == SyncProvider::Dropbox) {
        error = one_drive::rename_windows_dropbox_path(source, destination);
    } else
#endif
    {
        fs::rename(source, destination, error);
    }
    if (one_drive::is_retryable_move_error(error)) {
        return {.skipped = true, .message = one_drive::move_lock_message().toStdString()};
    }
    if (error) {
        return {.message = error.message()};
    }
    remove_empty_directories(created_directories);
    return {.success = true,
            .metadata = moved_metadata(destination, inspect_sync_path(destination, platform_, provider_),
                                       capture_content, provider_)};
#endif
}
