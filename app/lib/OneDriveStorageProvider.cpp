#include "OneDriveStorageProvider.hpp"

#include "CloudPathSupport.hpp"
#include "Utils.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#include <winternl.h>
#include <cfapi.h>
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
#else
#include <sys/stat.h>
#endif

namespace {

using RemoteMetadata = OneDriveStorageProvider::RemoteMetadata;

constexpr auto kOneDriveGraphAccessTokenEnv = "AI_FILE_SORTER_ONEDRIVE_GRAPH_ACCESS_TOKEN";
constexpr auto kOneDriveGraphDriveIdEnv = "AI_FILE_SORTER_ONEDRIVE_GRAPH_DRIVE_ID";
constexpr auto kOneDriveGraphBaseUrlEnv = "AI_FILE_SORTER_ONEDRIVE_GRAPH_BASE_URL";
constexpr auto kDefaultOneDriveGraphBaseUrl = "https://graph.microsoft.com/v1.0";

struct NativeIdentitySnapshot {
    bool available{false};
    std::string stable_identity;
    std::string revision_core;
};

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim_trailing_slashes(std::string value)
{
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string normalize_cache_key(const std::string& path)
{
    if (path.empty()) {
        return {};
    }

    try {
        auto normalized = Utils::path_to_utf8(Utils::utf8_to_path(path).lexically_normal());
#ifdef _WIN32
        normalized = to_lower_copy(std::move(normalized));
#endif
        return normalized;
    } catch (...) {
        return path;
    }
}

std::string url_encode_path(const std::string& value)
{
    const QByteArray encoded = QUrl::toPercentEncoding(QString::fromStdString(value), "/");
    return encoded.toStdString();
}

size_t write_to_string(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buffer = static_cast<std::string*>(userdata);
    const size_t bytes = size * nmemb;
    buffer->append(static_cast<const char*>(ptr), bytes);
    return bytes;
}

void configure_tls(CURL* curl)
{
#if defined(_WIN32)
    const auto cert_path = Utils::ensure_ca_bundle();
    curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
#else
    (void)curl;
#endif
}

std::optional<std::string> one_drive_relative_path(const std::filesystem::path& path,
                                                   const std::vector<std::string>& env_var_names)
{
    for (const auto& env_var_name : env_var_names) {
        const char* env_value = std::getenv(env_var_name.c_str());
        if (!env_value || *env_value == '\0') {
            continue;
        }

        const auto root = Utils::utf8_to_path(std::string(env_value)).lexically_normal();
        std::error_code ec;
        const auto relative = std::filesystem::relative(path.lexically_normal(), root, ec);
        if (ec) {
            continue;
        }

        const auto relative_text = Utils::path_to_utf8(relative);
        if (relative.empty() || relative_text == "." || relative_text.starts_with("..")) {
            if (path.lexically_normal() == root) {
                return std::string();
            }
            continue;
        }

        auto encoded_path = Utils::path_to_utf8(relative);
        std::replace(encoded_path.begin(), encoded_path.end(), '\\', '/');
        return encoded_path;
    }

    return std::nullopt;
}

std::optional<RemoteMetadata> fetch_graph_metadata(const std::string& path,
                                                   const std::vector<std::string>& env_var_names,
                                                   std::string* error)
{
    const char* token_env = std::getenv(kOneDriveGraphAccessTokenEnv);
    if (!token_env || *token_env == '\0') {
        return std::nullopt;
    }

    const auto relative_path = one_drive_relative_path(Utils::utf8_to_path(path), env_var_names);
    if (!relative_path.has_value()) {
        return std::nullopt;
    }

    std::string graph_base_url = kDefaultOneDriveGraphBaseUrl;
    if (const char* base_url_env = std::getenv(kOneDriveGraphBaseUrlEnv);
        base_url_env && *base_url_env != '\0') {
        graph_base_url = base_url_env;
    }
    graph_base_url = trim_trailing_slashes(std::move(graph_base_url));

    std::string request_url;
    const char* drive_id_env = std::getenv(kOneDriveGraphDriveIdEnv);
    if (drive_id_env && *drive_id_env != '\0') {
        request_url = graph_base_url + "/drives/" + std::string(drive_id_env) + "/root";
    } else {
        request_url = graph_base_url + "/me/drive/root";
    }
    if (!relative_path->empty()) {
        request_url += ":/" + url_encode_path(*relative_path);
    }
    request_url += "?$select=id,eTag,cTag,parentReference";

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = "Failed to initialize libcurl for OneDrive metadata lookup.";
        }
        return std::nullopt;
    }

    std::string response_body;
    struct curl_slist* headers = nullptr;
    const std::string auth_header = "Authorization: Bearer " + std::string(token_env);
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, request_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AI-File-Sorter-OneDrivePlugin/1.0");
    configure_tls(curl);

    const CURLcode result = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        if (error) {
            *error = "OneDrive Graph metadata request failed: " +
                std::string(curl_easy_strerror(result));
        }
        return std::nullopt;
    }

    if (http_code < 200 || http_code >= 300) {
        if (error) {
            *error = "OneDrive Graph metadata request returned HTTP " + std::to_string(http_code) + ".";
        }
        return std::nullopt;
    }

    const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response_body));
    if (!doc.isObject()) {
        if (error) {
            *error = "OneDrive Graph metadata response was not valid JSON.";
        }
        return std::nullopt;
    }

    const auto object = doc.object();
    const auto item_id = object.value("id").toString().toStdString();
    if (item_id.empty()) {
        if (error) {
            *error = "OneDrive Graph metadata response did not include an item id.";
        }
        return std::nullopt;
    }

    const auto parent_reference = object.value("parentReference").toObject();
    std::string drive_id = parent_reference.value("driveId").toString().toStdString();
    if (drive_id.empty() && drive_id_env && *drive_id_env != '\0') {
        drive_id = drive_id_env;
    }
    if (drive_id.empty()) {
        drive_id = "default";
    }

    return RemoteMetadata{
        .drive_id = std::move(drive_id),
        .item_id = item_id,
        .e_tag = object.value("eTag").toString().toStdString(),
        .c_tag = object.value("cTag").toString().toStdString()
    };
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

bool provider_name_looks_like_onedrive(const std::string& provider_name)
{
    return to_lower_copy(provider_name).find("onedrive") != std::string::npos;
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

std::uintmax_t read_size(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
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

struct ScopedHandle {
    HANDLE value{INVALID_HANDLE_VALUE};

    ~ScopedHandle()
    {
        if (value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }
};

NativeIdentitySnapshot read_native_identity_snapshot(const std::filesystem::path& path,
                                                     const OneDriveAttributeState& attributes)
{
    ScopedHandle handle{
        CreateFileW(path.c_str(),
                    FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    nullptr)
    };
    if (handle.value == INVALID_HANDLE_VALUE) {
        return {};
    }

    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle.value, &info)) {
        return {};
    }

    const std::uint64_t file_index =
        (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) |
        static_cast<std::uint64_t>(info.nFileIndexLow);
    const std::uint64_t size_bytes =
        (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) |
        static_cast<std::uint64_t>(info.nFileSizeLow);
    const std::uint64_t last_write =
        (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) |
        static_cast<std::uint64_t>(info.ftLastWriteTime.dwLowDateTime);

    NativeIdentitySnapshot snapshot;
    snapshot.available = true;

    std::ostringstream stable_stream;
    stable_stream << "onedrive:fileid:" << std::hex << std::setfill('0')
                  << std::setw(8) << info.dwVolumeSerialNumber
                  << ":" << std::setw(16) << file_index;
    snapshot.stable_identity = stable_stream.str();

    std::ostringstream revision_stream;
    revision_stream << snapshot.stable_identity
                    << ":" << std::dec << size_bytes
                    << ":" << last_write
                    << ":" << info.dwFileAttributes
                    << ":" << static_cast<int>(attributes.placeholder)
                    << static_cast<int>(attributes.partially_available)
                    << static_cast<int>(attributes.unpinned)
                    << static_cast<int>(attributes.pinned)
                    << static_cast<int>(attributes.temporary);
    snapshot.revision_core = revision_stream.str();
    return snapshot;
}

std::optional<OneDriveStorageProvider::SyncRootInfo> read_sync_root_info_by_path(
    const std::string& path,
    std::string* error)
{
    const auto native_path = Utils::utf8_to_path(path);

    CF_SYNC_ROOT_PROVIDER_INFO provider_info{};
    DWORD returned_length = 0;
    const HRESULT hr = CfGetSyncRootInfoByPath(native_path.c_str(),
                                               CF_SYNC_ROOT_INFO_PROVIDER,
                                               &provider_info,
                                               sizeof(provider_info),
                                               &returned_length);
    if (FAILED(hr)) {
        if (error) {
            *error = "Windows Cloud Files API could not resolve sync root information.";
        }
        return std::nullopt;
    }

    std::string provider_name = QString::fromWCharArray(provider_info.ProviderName).toStdString();
    std::string provider_version =
        QString::fromWCharArray(provider_info.ProviderVersion).toStdString();
    if (provider_name.empty()) {
        if (error) {
            *error = "Windows Cloud Files API did not report a provider name.";
        }
        return std::nullopt;
    }

    return OneDriveStorageProvider::SyncRootInfo{
        .provider_name = std::move(provider_name),
        .provider_version = std::move(provider_version)
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

std::uint64_t timespec_to_nanoseconds(const timespec& ts)
{
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

NativeIdentitySnapshot read_native_identity_snapshot(const std::filesystem::path& path,
                                                     const OneDriveAttributeState& attributes)
{
    struct stat info {};
    if (::stat(path.c_str(), &info) != 0) {
        return {};
    }

    NativeIdentitySnapshot snapshot;
    snapshot.available = true;

    std::ostringstream stable_stream;
    stable_stream << "onedrive:inode:" << info.st_dev << ":" << info.st_ino;
    snapshot.stable_identity = stable_stream.str();

    std::uint64_t mtime_ns = 0;
#if defined(__APPLE__)
    mtime_ns = timespec_to_nanoseconds(info.st_mtimespec);
#elif defined(__linux__)
    mtime_ns = timespec_to_nanoseconds(info.st_mtim);
#else
    mtime_ns = static_cast<std::uint64_t>(info.st_mtime) * 1000000000ull;
#endif

    std::ostringstream revision_stream;
    revision_stream << snapshot.stable_identity
                    << ":" << static_cast<std::uintmax_t>(info.st_size)
                    << ":" << mtime_ns
                    << ":" << static_cast<unsigned int>(info.st_mode)
                    << ":" << static_cast<int>(attributes.placeholder)
                    << static_cast<int>(attributes.partially_available)
                    << static_cast<int>(attributes.unpinned)
                    << static_cast<int>(attributes.pinned)
                    << static_cast<int>(attributes.temporary);
    snapshot.revision_core = revision_stream.str();
    return snapshot;
}

std::optional<OneDriveStorageProvider::SyncRootInfo> read_sync_root_info_by_path(
    const std::string&,
    std::string*)
{
    return std::nullopt;
}
#endif

} // namespace

OneDriveStorageProvider::OneDriveStorageProvider()
    : OneDriveStorageProvider(RemoteMetadataResolver{}, SyncRootResolver{})
{
}

OneDriveStorageProvider::OneDriveStorageProvider(RemoteMetadataResolver remote_metadata_resolver)
    : OneDriveStorageProvider(std::move(remote_metadata_resolver), SyncRootResolver{})
{
}

OneDriveStorageProvider::OneDriveStorageProvider(SyncRootResolver sync_root_resolver)
    : OneDriveStorageProvider(RemoteMetadataResolver{}, std::move(sync_root_resolver))
{
}

OneDriveStorageProvider::OneDriveStorageProvider(RemoteMetadataResolver remote_metadata_resolver,
                                                 SyncRootResolver sync_root_resolver)
    : path_markers_({"onedrive"}),
      env_var_names_({"OneDrive", "OneDriveCommercial", "OneDriveConsumer"}),
      remote_metadata_resolver_(std::move(remote_metadata_resolver)),
      sync_root_resolver_(std::move(sync_root_resolver))
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
    std::string sync_root_error;
    if (const auto sync_root = query_sync_root_info(root_path, &sync_root_error);
        sync_root.has_value()) {
        if (provider_name_looks_like_onedrive(sync_root->provider_name)) {
            StorageProviderDetection detection{
                .provider_id = id(),
                .matched = true,
                .needs_additional_support = false,
                .confidence = 160,
                .detection_source = "windows_sync_root",
                .message = "Windows identified this folder as a OneDrive sync root."
            };
            if (!sync_root->provider_version.empty()) {
                detection.message += " Provider version: " + sync_root->provider_version + ".";
            }
            return detection;
        }
        return {};
    }

    const CloudPathMatch match = detect_cloud_path_match(root_path, path_markers_, env_var_names_);
    if (!match.matched || match.confidence <= 0) {
        return {};
    }

    return StorageProviderDetection{
        .provider_id = id(),
        .matched = true,
        .needs_additional_support = false,
        .confidence = match.confidence + 30,
        .detection_source = "path_heuristic",
        .message = "Detected a OneDrive folder by path/root matching. Dedicated compatibility support is active."
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
    const auto fs_path = Utils::utf8_to_path(path);
    StoragePathStatus status;
    status.exists = path_exists_impl(fs_path);
    status.stable_identity = make_stable_identity(path);
    if (!status.exists) {
        status.message = "Path does not exist.";
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

    const auto native_snapshot = read_native_identity_snapshot(fs_path, attributes);
    if (native_snapshot.available) {
        status.stable_identity = native_snapshot.stable_identity;
    }

    std::string remote_metadata_error;
    if (const auto remote_metadata = query_remote_metadata(path, &remote_metadata_error);
        remote_metadata.has_value()) {
        status.stable_identity =
            "onedrive:item:" + remote_metadata->drive_id + ":" + remote_metadata->item_id;
        status.revision_token =
            "onedrive:rev:" + remote_metadata->drive_id + ":" + remote_metadata->item_id +
            ":" + (remote_metadata->e_tag.empty() ? "-" : remote_metadata->e_tag) +
            ":" + (remote_metadata->c_tag.empty() ? "-" : remote_metadata->c_tag) +
            ":" + (status.hydration_required ? "offline" : "local") +
            ":" + (status.sync_locked ? "locked" : "idle");
        return status;
    } else if (!remote_metadata_error.empty() && status.message.empty()) {
        status.message = remote_metadata_error;
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
    return path_exists_impl(Utils::utf8_to_path(path));
}

bool OneDriveStorageProvider::ensure_directory(const std::string& directory, std::string* error) const
{
    return ensure_directory_impl(Utils::utf8_to_path(directory), error);
}

StorageMutationResult OneDriveStorageProvider::move_entry(const std::string& source,
                                                          const std::string& destination) const
{
    StorageMutationResult result;
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

    const auto source_path = Utils::utf8_to_path(source);
    const auto destination_path = Utils::utf8_to_path(destination);

    std::string ensure_error;
    if (!ensure_directory_impl(destination_path.parent_path(), &ensure_error)) {
        result.message = ensure_error.empty()
            ? "Failed to create destination directories."
            : ensure_error;
        result.metadata.stable_identity = preflight.source_status.stable_identity;
        result.metadata.revision_token = preflight.source_status.revision_token;
        return result;
    }

    std::error_code ec;
    std::filesystem::rename(source_path, destination_path, ec);
    if (ec) {
        const auto source_status = inspect_local_path(source);
        const auto destination_status = inspect_local_path(destination);
        result.message = source_status.sync_locked
            ? source_status.message
            : (destination_status.sync_locked ? destination_status.message : ec.message());
        result.metadata.stable_identity = preflight.source_status.stable_identity;
        result.metadata.revision_token = preflight.source_status.revision_token;
        return result;
    }

    const auto destination_status = inspect_local_path(destination);
    result.success = true;
    result.metadata = build_metadata(destination_path, destination_status);
    return result;
}

StorageMutationResult OneDriveStorageProvider::undo_move(const std::string& source,
                                                         const std::string& destination) const
{
    StorageMutationResult result;
    const auto source_path = Utils::utf8_to_path(source);
    const auto destination_path = Utils::utf8_to_path(destination);
    const auto source_status = inspect_local_path(source);
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

    if (destination_status.hydration_required || destination_status.sync_locked) {
        return StorageMutationResult{
            .success = false,
            .skipped = false,
            .message = destination_status.message.empty()
                ? "OneDrive has not finished syncing the moved file."
                : destination_status.message
        };
    }

    std::string ensure_error;
    if (!ensure_directory_impl(source_path.parent_path(), &ensure_error)) {
        result.message = ensure_error.empty()
            ? "Failed to create source directories."
            : ensure_error;
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
    result.metadata = build_metadata(source_path, inspect_local_path(source));
    return result;
}

StorageEntryMetadata OneDriveStorageProvider::build_metadata(const std::filesystem::path& path,
                                                             const StoragePathStatus& status) const
{
    StorageEntryMetadata metadata;
    metadata.size_bytes = read_size(path);
    metadata.mtime = read_mtime(path);
    metadata.stable_identity = status.stable_identity.empty()
        ? make_stable_identity(Utils::path_to_utf8(path))
        : status.stable_identity;
    metadata.revision_token = status.revision_token.empty()
        ? make_revision_token(path, status)
        : status.revision_token;
    return metadata;
}

std::optional<OneDriveStorageProvider::SyncRootInfo> OneDriveStorageProvider::query_sync_root_info(
    const std::string& path,
    std::string* error) const
{
    const auto cache_key = normalize_cache_key(path);
    const auto cached = sync_root_info_cache_.find(cache_key);
    if (cached != sync_root_info_cache_.end()) {
        if (error) {
            error->clear();
        }
        return cached->second;
    }

    std::optional<SyncRootInfo> result;
    if (sync_root_resolver_) {
        result = sync_root_resolver_(path, error);
    } else {
        result = resolve_sync_root_info(path, error);
    }

    if (result.has_value() || !error || error->empty()) {
        sync_root_info_cache_.emplace(cache_key, result);
    }
    return result;
}

std::optional<RemoteMetadata> OneDriveStorageProvider::query_remote_metadata(const std::string& path,
                                                                             std::string* error) const
{
    if (remote_metadata_resolver_) {
        return remote_metadata_resolver_(path, error);
    }
    return resolve_graph_metadata(path, env_var_names_, error);
}

std::optional<OneDriveStorageProvider::SyncRootInfo> OneDriveStorageProvider::resolve_sync_root_info(
    const std::string& path,
    std::string* error)
{
    return read_sync_root_info_by_path(path, error);
}

bool OneDriveStorageProvider::path_exists_impl(const std::filesystem::path& path) const
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool OneDriveStorageProvider::ensure_directory_impl(const std::filesystem::path& directory,
                                                    std::string* error) const
{
    if (directory.empty()) {
        if (error) {
            *error = "Directory path is empty.";
        }
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    return true;
}

std::string OneDriveStorageProvider::make_revision_token(const std::filesystem::path& path,
                                                        const StoragePathStatus& status) const
{
    const auto native_snapshot = read_native_identity_snapshot(path, read_onedrive_attributes(path));
    const std::string base_revision = native_snapshot.available
        ? native_snapshot.revision_core
        : ("onedrive:synthetic:" + std::to_string(read_size(path)) + ":" + std::to_string(read_mtime(path)));
    return base_revision + ":" +
           (status.hydration_required ? "offline" : "local") + ":" +
           (status.sync_locked ? "locked" : "idle");
}

std::string OneDriveStorageProvider::make_stable_identity(const std::string& path) const
{
    const auto fs_path = Utils::utf8_to_path(path);
    const auto native_snapshot = read_native_identity_snapshot(fs_path, read_onedrive_attributes(fs_path));
    if (native_snapshot.available) {
        return native_snapshot.stable_identity;
    }

    const auto normalized = Utils::path_to_utf8(fs_path.lexically_normal());
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

std::optional<RemoteMetadata> OneDriveStorageProvider::resolve_graph_metadata(
    const std::string& path,
    const std::vector<std::string>& env_var_names,
    std::string* error)
{
    return fetch_graph_metadata(path, env_var_names, error);
}
