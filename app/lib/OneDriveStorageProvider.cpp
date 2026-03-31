#include "OneDriveStorageProvider.hpp"

#include "CloudPathSupport.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif
#ifndef FILE_ATTRIBUTE_PINNED
#define FILE_ATTRIBUTE_PINNED 0x00080000
#endif
#ifndef FILE_ATTRIBUTE_UNPINNED
#define FILE_ATTRIBUTE_UNPINNED 0x00100000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif
#endif

namespace {

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool looks_like_onedrive_lock_file(const std::filesystem::path& path)
{
    const std::string name = to_lower_copy(path.filename().string());
    return name.starts_with("~$") ||
           name.ends_with(".tmp") ||
           name.ends_with(".part") ||
           name.ends_with(".partial") ||
           name.ends_with(".download") ||
           name.find("conflicted copy") != std::string::npos;
}

bool contains_sync_temp_component(const std::filesystem::path& path)
{
    static const std::unordered_set<std::string> one_drive_temp_components = {
        ".tmp.drivedownload",
        ".tmp.driveupload",
        ".odcontainer",
        "sync issues"
    };

    for (const auto& component : path) {
        const auto lowered = to_lower_copy(component.string());
        if (one_drive_temp_components.contains(lowered)) {
            return true;
        }
    }
    return false;
}

std::time_t read_mtime(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return 0;
    }

    const auto delta = write_time - std::filesystem::file_time_type::clock::now();
    const auto system_time = std::chrono::system_clock::now() +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(delta);
    return std::chrono::system_clock::to_time_t(system_time);
}

#ifdef _WIN32
struct OneDriveAttributeState {
    bool placeholder{false};
    bool partially_available{false};
    bool unpinned{false};
    bool pinned{false};
    bool temporary{false};
};

OneDriveAttributeState read_onedrive_attributes(const std::filesystem::path& path)
{
    const auto native = path.native();
    const DWORD attrs = GetFileAttributesW(native.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return {};
    }

    return OneDriveAttributeState{
        .placeholder = (attrs & FILE_ATTRIBUTE_OFFLINE) != 0 ||
            (attrs & FILE_ATTRIBUTE_RECALL_ON_OPEN) != 0,
        .partially_available = (attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) != 0,
        .unpinned = (attrs & FILE_ATTRIBUTE_UNPINNED) != 0,
        .pinned = (attrs & FILE_ATTRIBUTE_PINNED) != 0,
        .temporary = (attrs & FILE_ATTRIBUTE_TEMPORARY) != 0
    };
}
#else
struct OneDriveAttributeState {
    bool placeholder{false};
    bool partially_available{false};
    bool unpinned{false};
    bool pinned{false};
    bool temporary{false};
};

OneDriveAttributeState read_onedrive_attributes(const std::filesystem::path&)
{
    return {};
}
#endif

} // namespace

OneDriveStorageProvider::OneDriveStorageProvider()
    : path_markers_({"onedrive"}),
      env_var_names_({"OneDrive", "OneDriveCommercial", "OneDriveConsumer"})
{
    scan_behavior_.skip_reparse_points = true;
    scan_behavior_.junk_name_prefixes = {"~$"};
}

std::string OneDriveStorageProvider::id() const
{
    return "onedrive";
}

StorageProviderDetection OneDriveStorageProvider::detect(const std::string& root_path) const
{
    const CloudPathMatch match = detect_cloud_path_match(root_path, path_markers_, env_var_names_);
    if (!match.matched || match.confidence <= 0) {
        return {};
    }

    return StorageProviderDetection{
        .provider_id = id(),
        .matched = true,
        .needs_additional_support = false,
        .confidence = match.confidence + 30,
        .message = "Detected a OneDrive folder. Dedicated compatibility support is active."
    };
}

StorageProviderCapabilities OneDriveStorageProvider::capabilities() const
{
    return StorageProviderCapabilities{
        .supports_online_only_files = true,
        .supports_atomic_rename = false,
        .should_skip_reparse_points = true,
        .should_relax_undo_mtime_validation = true
    };
}

std::vector<FileEntry> OneDriveStorageProvider::list_directory(const std::string& directory,
                                                               FileScanOptions options) const
{
    return scanner_.get_directory_entries(directory, options, scan_behavior_);
}

StoragePathStatus OneDriveStorageProvider::inspect_local_path(const std::string& path) const
{
    StoragePathStatus status = fallback_provider_.inspect_path(path);
    status.stable_identity = make_stable_identity(path);
    const auto fs_path = Utils::utf8_to_path(path);
    if (!status.exists) {
        return status;
    }

    status.conflict_copy = looks_like_onedrive_lock_file(fs_path);
    if (status.conflict_copy || contains_sync_temp_component(fs_path)) {
        status.sync_locked = true;
        status.should_retry = true;
        status.retry_after_ms = 5000;
        status.message = contains_sync_temp_component(fs_path)
            ? "OneDrive is still staging this item in a temporary sync location."
            : "OneDrive lock or conflict file is still present.";
    }

    const OneDriveAttributeState attributes = read_onedrive_attributes(fs_path);
    if (attributes.placeholder || attributes.partially_available) {
        status.hydration_required = true;
        status.should_retry = true;
        status.retry_after_ms = std::max(status.retry_after_ms, 5000);
        if (attributes.placeholder) {
            status.message = "OneDrive reports this file as online-only. Hydrate it before moving.";
        } else {
            status.message = "OneDrive reports this file as partially local. Finish hydration before moving.";
        }
    } else if (attributes.unpinned && !attributes.pinned && status.message.empty()) {
        status.message = "OneDrive marks this item as unpinned; it may be evicted while syncing.";
    }

    if (attributes.temporary && !status.sync_locked) {
        status.sync_locked = true;
        status.should_retry = true;
        status.retry_after_ms = std::max(status.retry_after_ms, 2000);
        if (status.message.empty()) {
            status.message = "OneDrive is still updating this temporary file.";
        }
    }

    status.revision_token = make_revision_token(fs_path, status);
    return status;
}

StoragePathStatus OneDriveStorageProvider::inspect_path(const std::string& path) const
{
    return inspect_local_path(path);
}

StorageMovePreflight OneDriveStorageProvider::preflight_move(const std::string& source,
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

    if (preflight.source_status.hydration_required) {
        preflight.allowed = false;
        preflight.hydration_required = true;
        preflight.should_retry = preflight.source_status.should_retry;
        preflight.retry_after_ms = preflight.source_status.retry_after_ms;
        preflight.message = preflight.source_status.message;
        return preflight;
    }

    if (preflight.source_status.sync_locked) {
        preflight.allowed = false;
        preflight.sync_locked = true;
        preflight.should_retry = preflight.source_status.should_retry;
        preflight.retry_after_ms = preflight.source_status.retry_after_ms;
        preflight.message = preflight.source_status.message.empty()
            ? "OneDrive indicates the file is still being synchronized."
            : preflight.source_status.message;
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

bool OneDriveStorageProvider::path_exists(const std::string& path) const
{
    return fallback_provider_.path_exists(path);
}

bool OneDriveStorageProvider::ensure_directory(const std::string& directory, std::string* error) const
{
    return fallback_provider_.ensure_directory(directory, error);
}

StorageMutationResult OneDriveStorageProvider::move_entry(const std::string& source,
                                                          const std::string& destination) const
{
    const auto preflight = preflight_move(source, destination);
    if (!preflight.allowed) {
        return StorageMutationResult{
            .success = false,
            .skipped = preflight.skipped,
            .message = preflight.message,
            .metadata = {
                .size_bytes = 0,
                .mtime = 0,
                .stable_identity = preflight.source_status.stable_identity,
                .revision_token = preflight.source_status.revision_token
            }
        };
    }

    auto result = fallback_provider_.move_entry(source, destination);
    if (result.success) {
        const auto destination_status = inspect_path(destination);
        result.metadata.stable_identity = destination_status.stable_identity;
        result.metadata.revision_token = destination_status.revision_token;
    }
    return result;
}

StorageMutationResult OneDriveStorageProvider::undo_move(const std::string& source,
                                                         const std::string& destination) const
{
    const auto destination_status = inspect_path(destination);
    if (destination_status.hydration_required || destination_status.sync_locked) {
        return StorageMutationResult{
            .success = false,
            .skipped = false,
            .message = destination_status.message.empty()
                ? "OneDrive has not finished syncing the moved file."
                : destination_status.message
        };
    }

    return fallback_provider_.undo_move(source, destination);
}

std::string OneDriveStorageProvider::make_revision_token(const std::filesystem::path& path,
                                                        const StoragePathStatus& status) const
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    const auto mtime = read_mtime(path);
    return std::to_string(status.exists ? 1 : 0) + ":" +
           std::to_string(ec ? 0 : size) + ":" +
           std::to_string(mtime) + ":" +
           (status.hydration_required ? "offline" : "local") + ":" +
           (status.sync_locked ? "locked" : "idle");
}

std::string OneDriveStorageProvider::make_stable_identity(const std::string& path) const
{
    const auto normalized = Utils::path_to_utf8(Utils::utf8_to_path(path).lexically_normal());
    for (const auto& env_var_name : env_var_names_) {
        const char* env_value = std::getenv(env_var_name.c_str());
        if (!env_value || *env_value == '\0') {
            continue;
        }

        const auto root = Utils::utf8_to_path(std::string(env_value)).lexically_normal();
        const auto candidate = Utils::utf8_to_path(normalized);
        std::error_code ec;
        const auto relative = std::filesystem::relative(candidate, root, ec);
        const auto relative_text = Utils::path_to_utf8(relative);
        if (!ec &&
            !relative.empty() &&
            relative_text != "." &&
            !relative_text.starts_with("..")) {
            return "onedrive:" + relative_text;
        }
        if (!ec && candidate == root) {
            return "onedrive:.";
        }
    }
    return "onedrive:" + normalized;
}
