#include "OneDrivePlatform.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <array>
#include <optional>
#include <system_error>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winternl.h>
#include <cfapi.h>
// clang-format on

#include <vector>
#elif defined(__APPLE__)
#include "OneDriveMacPathInspection.hpp"
#include "OneDriveMacPaths.hpp"
#include "OneDriveMacRootDetection.hpp"
#include "OneDriveMacRevision.hpp"
#endif

namespace one_drive {
namespace {

#ifndef __APPLE__
constexpr std::array<const char *, 3> kOneDriveEnvVars{
    "OneDrive", "OneDriveCommercial", "OneDriveConsumer"};

QString clean_for_compare(QString text) {
  text = QDir::fromNativeSeparators(text);
  text = QDir::cleanPath(text);
  while (text.size() > 1 && text.endsWith(QLatin1Char('/'))) {
    text.chop(1);
  }
  return text.toCaseFolded();
}

bool path_is_same_or_under(const QString &path, const QString &root) {
  const QString clean_path = clean_for_compare(path);
  const QString clean_root = clean_for_compare(root);
  if (clean_root.isEmpty()) {
    return false;
  }
  return clean_path == clean_root ||
         clean_path.startsWith(clean_root + QStringLiteral("/"));
}

bool path_is_under_env_root(const std::filesystem::path &path) {
  const QString path_text = normalized_path_text(path);
  for (const char *name : kOneDriveEnvVars) {
    const QString root = QString::fromLocal8Bit(qgetenv(name));
    if (!root.isEmpty() && path_is_same_or_under(path_text, root)) {
      return true;
    }
  }
  return false;
}

bool path_has_onedrive_component(const std::filesystem::path &path) {
  const QStringList parts =
      normalized_path_text(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    if (part.contains(QStringLiteral("OneDrive"), Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}
#endif

QString stable_path_identity(const std::filesystem::path &path) {
  return QStringLiteral("onedrive:path:%1").arg(normalized_path_text(path));
}

#if !defined(_WIN32) && !defined(__APPLE__)
QString local_revision_suffix(qint64 size_bytes, qint64 mtime) {
  return QStringLiteral("local:%1:%2").arg(size_bytes).arg(mtime);
}
#endif

bool filename_looks_like_office_lock(const QString &filename) {
  return filename.startsWith(QStringLiteral("~$"));
}

bool filename_looks_like_temporary_sync_file(const QString &filename) {
  const QString lower = filename.toCaseFolded();
  return lower.endsWith(QStringLiteral(".partial")) ||
         lower.endsWith(QStringLiteral(".tmp")) ||
         lower.endsWith(QStringLiteral(".download")) ||
         lower.endsWith(QStringLiteral(".upload")) ||
         lower.endsWith(QStringLiteral(".sync"));
}

bool filename_looks_like_conflict_copy(const QString &filename) {
  const QString lower = filename.toCaseFolded();
  return lower.contains(QStringLiteral("conflicted copy")) ||
         lower.contains(QStringLiteral("conflict copy")) ||
         lower.contains(QStringLiteral("conflict-"));
}

bool path_has_staging_component(const std::filesystem::path &path) {
  for (const auto &part : path) {
    const QString value = path_to_qstring(part).toCaseFolded();
    if (value == QStringLiteral(".tmp.drivedownload") ||
        value == QStringLiteral(".tmp.driveupload") ||
        value == QStringLiteral(".tmp") ||
        value == QStringLiteral(".odupload") ||
        value == QStringLiteral(".oddownload")) {
      return true;
    }
  }
  return false;
}

bool path_name_is_sync_locked(const std::filesystem::path &path) {
  const QString filename = path_to_qstring(path.filename());
  return filename_looks_like_office_lock(filename) ||
         filename_looks_like_temporary_sync_file(filename) ||
         path_has_staging_component(path);
}

#ifdef _WIN32

struct WinHandle {
  HANDLE value{INVALID_HANDLE_VALUE};

  explicit WinHandle(HANDLE handle) : value(handle) {}
  WinHandle(const WinHandle &) = delete;
  WinHandle &operator=(const WinHandle &) = delete;

  ~WinHandle() {
    if (value != INVALID_HANDLE_VALUE && value != nullptr) {
      CloseHandle(value);
    }
  }

  bool valid() const {
    return value != INVALID_HANDLE_VALUE && value != nullptr;
  }
};

std::wstring to_wide_path(const std::filesystem::path &path) {
  return path.wstring();
}

QString wide_to_qstring(const wchar_t *value) {
  return value ? QString::fromWCharArray(value) : QString();
}

bool provider_name_is_onedrive(const QString &provider_name) {
  return provider_name.contains(QStringLiteral("OneDrive"),
                                Qt::CaseInsensitive) ||
         provider_name.contains(QStringLiteral("Microsoft OneDrive"),
                                Qt::CaseInsensitive);
}

std::optional<CF_SYNC_ROOT_PROVIDER_INFO>
query_sync_root_provider(const std::filesystem::path &path) {
  CF_SYNC_ROOT_PROVIDER_INFO info{};
  DWORD returned_length = 0;
  const std::wstring wide_path = to_wide_path(path);
  const HRESULT hr =
      CfGetSyncRootInfoByPath(wide_path.c_str(), CF_SYNC_ROOT_INFO_PROVIDER,
                              &info, sizeof(info), &returned_length);
  if (SUCCEEDED(hr)) {
    return info;
  }
  return std::nullopt;
}

SyncRootDetection
detect_windows_cloud_files_root(const std::filesystem::path &path, SyncProvider expected) {
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  if (error) {
    return {};
  }
  const auto registered = query_sync_root_provider(absolute);
  if (!registered) {
    return {};
  }

  SyncRootDetection detection;
  detection.authoritative = true;
  detection.source = QStringLiteral("windows_cloud_files");
  detection.provider_name = wide_to_qstring(registered->ProviderName);

  const bool dropbox = expected == SyncProvider::Dropbox;
  const bool matched = dropbox
      ? detection.provider_name.trimmed().compare(QStringLiteral("Dropbox"), Qt::CaseInsensitive) == 0
      : provider_name_is_onedrive(detection.provider_name);
  if (matched) {
    detection.matched = true;
    detection.confidence = 95;
    detection.message = dropbox ? QStringLiteral("Detected Dropbox via Windows Cloud Files.")
                                : QStringLiteral("Detected OneDrive via Windows Cloud Files.");
    return detection;
  }

  detection.vetoed = true;
  detection.confidence = 0;
  detection.message =
      detection.provider_name.isEmpty()
          ? QStringLiteral(
                "Windows Cloud Files identified a different sync provider.")
          : QStringLiteral("Windows Cloud Files identified sync provider '%1'.")
                .arg(detection.provider_name);
  return detection;
}

std::optional<WIN32_FIND_DATAW>
find_data_for_path(const std::filesystem::path &path,
                   std::error_code *error = nullptr) {
  WIN32_FIND_DATAW data{};
  const std::wstring wide_path = to_wide_path(path);
  HANDLE handle = FindFirstFileW(wide_path.c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    if (error) {
      *error = std::error_code(static_cast<int>(GetLastError()),
                               std::system_category());
    }
    return std::nullopt;
  }
  FindClose(handle);
  return data;
}

WinHandle open_metadata_handle(const std::filesystem::path &path) {
  const std::wstring wide_path = to_wide_path(path);
  return WinHandle(CreateFileW(
      wide_path.c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
      nullptr));
}

QString file_id_identity(const BY_HANDLE_FILE_INFORMATION &info) {
  const quint64 file_index =
      (static_cast<quint64>(info.nFileIndexHigh) << 32U) |
      static_cast<quint64>(info.nFileIndexLow);
  if (file_index == 0) {
    return {};
  }
  return QStringLiteral("onedrive:fileid:%1:%2")
      .arg(static_cast<quint32>(info.dwVolumeSerialNumber), 8, 16,
           QLatin1Char('0'))
      .arg(file_index, 16, 16, QLatin1Char('0'));
}

QString placeholder_identity(HANDLE handle) {
  DWORD returned_length = 0;
  std::vector<BYTE> buffer(sizeof(CF_PLACEHOLDER_BASIC_INFO) +
                           CF_PLACEHOLDER_MAX_FILE_IDENTITY_LENGTH);
  HRESULT hr = CfGetPlaceholderInfo(
      handle, CF_PLACEHOLDER_INFO_BASIC, buffer.data(),
      static_cast<DWORD>(buffer.size()), &returned_length);
  if (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA) && returned_length > 0) {
    buffer.resize(returned_length);
    hr = CfGetPlaceholderInfo(handle, CF_PLACEHOLDER_INFO_BASIC,
                              buffer.data(), static_cast<DWORD>(buffer.size()),
                              &returned_length);
  }
  if (FAILED(hr)) {
    return {};
  }

  const auto *basic =
      reinterpret_cast<const CF_PLACEHOLDER_BASIC_INFO *>(buffer.data());
  if (basic->FileIdentityLength == 0) {
    return {};
  }

  const QByteArray raw_identity(
      reinterpret_cast<const char *>(basic->FileIdentity),
      static_cast<qsizetype>(basic->FileIdentityLength));
  const QString encoded_identity = QString::fromLatin1(raw_identity.toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

  return QStringLiteral("onedrive:cf:%1").arg(encoded_identity);
}

bool attributes_indicate_hydration_required(DWORD attributes,
                                            CF_PLACEHOLDER_STATE state) {
  const bool recalls_on_access =
      (attributes & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) != 0;
  const bool recalls_on_open =
      (attributes & FILE_ATTRIBUTE_RECALL_ON_OPEN) != 0;
  const bool partial = (state & CF_PLACEHOLDER_STATE_PARTIAL) != 0 ||
                       (state & CF_PLACEHOLDER_STATE_PARTIALLY_ON_DISK) != 0;
  return recalls_on_access || recalls_on_open || partial ||
         (attributes & FILE_ATTRIBUTE_OFFLINE) != 0;
}

QString write_revision(const FILETIME &write_time, DWORD size_high,
                       DWORD size_low) {
  ULARGE_INTEGER ticks{};
  ticks.HighPart = write_time.dwHighDateTime;
  ticks.LowPart = write_time.dwLowDateTime;
  if (ticks.QuadPart == 0) {
    return {};
  }
  ULARGE_INTEGER size{};
  size.HighPart = size_high;
  size.LowPart = size_low;
  return QStringLiteral("write-v1:%1:%2").arg(ticks.QuadPart).arg(size.QuadPart);
}

bool can_read_local_content(DWORD attributes, DWORD reparse_tag) {
  const auto state = CfGetPlaceholderStateFromAttributeTag(attributes, reparse_tag);
  return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
         state != CF_PLACEHOLDER_STATE_INVALID &&
         ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 ||
          (state & CF_PLACEHOLDER_STATE_PLACEHOLDER) != 0) &&
         !attributes_indicate_hydration_required(attributes, state);
}

QString hash_data_stream(HANDLE handle, const BY_HANDLE_FILE_INFORMATION &info) {
  constexpr DWORD kReadBufferSize = 256 * 1024;
  QByteArray buffer(kReadBufferSize, Qt::Uninitialized);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  ULARGE_INTEGER file_size{};
  file_size.HighPart = info.nFileSizeHigh;
  file_size.LowPart = info.nFileSizeLow;
  auto remaining = file_size.QuadPart;
  while (remaining > 0) {
    DWORD read = 0;
    const auto count = static_cast<DWORD>(std::min<quint64>(remaining, kReadBufferSize));
    if (!ReadFile(handle, buffer.data(), count, &read, nullptr) || read == 0) {
      return {};
    }
    hash.addData(QByteArrayView(buffer.constData(), read));
    remaining -= read;
  }
  return QStringLiteral("onedrive:sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

bool has_windows_move_lock(const std::filesystem::path &path) {
  const WinHandle handle(CreateFileW(
      path.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  return !handle.valid() && is_retryable_move_error(
      std::error_code(static_cast<int>(GetLastError()), std::system_category()));
}

PlatformFileInfo inspect_windows_path(const std::filesystem::path &path) {
  PlatformFileInfo info;
  const WinHandle handle = open_metadata_handle(path);
  if (handle.valid()) {
    BY_HANDLE_FILE_INFORMATION native{};
    if (GetFileInformationByHandle(handle.value, &native)) {
      info.stable_identity = file_id_identity(native);
      info.revision_suffix = write_revision(native.ftLastWriteTime,
                                             native.nFileSizeHigh,
                                             native.nFileSizeLow);
    }
    if (info.stable_identity.isEmpty()) {
      info.stable_identity = placeholder_identity(handle.value);
    }
  }

  const auto data = find_data_for_path(path);
  if (data) {
    const CF_PLACEHOLDER_STATE state = CfGetPlaceholderStateFromAttributeTag(
        data->dwFileAttributes, data->dwReserved0);
    info.hydration_required =
        attributes_indicate_hydration_required(data->dwFileAttributes, state);
    info.sync_locked = (data->dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0;
    if (info.revision_suffix.isEmpty()) {
      info.revision_suffix =
          write_revision(data->ftLastWriteTime, data->nFileSizeHigh,
                         data->nFileSizeLow);
    }
  }
  if (!info.sync_locked && has_windows_move_lock(path)) {
    info.sync_locked = true;
    info.message = move_lock_message();
  }

  return info;
}

#endif

class DefaultPlatformProbe final : public PlatformProbe {
public:
  explicit DefaultPlatformProbe(SyncProvider provider) : provider_(provider) {}

  SyncRootDetection
  detect_sync_root(const std::filesystem::path &path) const override {
#ifdef _WIN32
    return detect_windows_cloud_files_root(path, provider_);
#elif defined(__APPLE__)
    return detect_macos_sync_root(path, provider_);
#else
    Q_UNUSED(path);
    return {};
#endif
  }

  PathInspection inspect_path(const std::filesystem::path &path) const override {
#ifdef __APPLE__
    return inspect_macos_path(path);
#else
    auto inspection = PlatformProbe::inspect_path(path);
#ifdef _WIN32
    if (inspection.exists) {
      inspection.state = inspect_windows_path(path);
    }
#endif
    return inspection;
#endif
  }

private:
  [[maybe_unused]] SyncProvider provider_;
};

} // namespace

PathInspection PlatformProbe::inspect_path(const std::filesystem::path &path) const {
  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  PathInspection inspection;
  inspection.exists = !error && std::filesystem::exists(status);
  if (!inspection.exists) {
    return inspection;
  }
  if (std::filesystem::is_regular_file(status)) {
    const auto size = std::filesystem::file_size(path, error);
    if (!error) {
      inspection.size_bytes = static_cast<qint64>(size);
    }
  }
  inspection.mtime =
      QFileInfo(path_to_qstring(path)).lastModified().toSecsSinceEpoch();
  return inspection;
}

std::filesystem::path path_from_qstring(const QString &value) {
#ifdef _WIN32
  return std::filesystem::path(value.toStdWString());
#else
  return std::filesystem::path(value.toStdString());
#endif
}

QString path_to_qstring(const std::filesystem::path &path) {
#ifdef _WIN32
  return QString::fromStdWString(path.wstring());
#else
  return QString::fromStdString(path.string());
#endif
}

QString normalized_path_text(const std::filesystem::path &path) {
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  return QDir::cleanPath(
      QDir::fromNativeSeparators(path_to_qstring(ec ? path : absolute)));
}

ScanEntryInfo scan_entry_info(const std::filesystem::path &path) {
  const bool dot_hidden = path_to_qstring(path.filename()).startsWith(QLatin1Char('.'));
#ifdef _WIN32
  std::error_code error;
  const auto data = find_data_for_path(path, &error);
  if (!data) {
    throw std::filesystem::filesystem_error("Read directory entry", path, error);
  }
  const DWORD attributes = data->dwFileAttributes;
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    const auto state = CfGetPlaceholderStateFromAttributeTag(attributes, data->dwReserved0);
    if ((state & (CF_PLACEHOLDER_STATE_PLACEHOLDER | CF_PLACEHOLDER_STATE_SYNC_ROOT)) == 0 ||
        state == CF_PLACEHOLDER_STATE_INVALID) {
      return {};
    }
  }
  const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return {dot_hidden || (attributes & FILE_ATTRIBUTE_HIDDEN) != 0,
          directory, !directory};
#elif defined(__APPLE__)
  return scan_macos_entry(path, dot_hidden);
#else
  const auto status = std::filesystem::symlink_status(path);
  return {dot_hidden, std::filesystem::is_directory(status),
          std::filesystem::is_regular_file(status)};
#endif
}

bool is_retryable_move_error(const std::error_code &error) {
#ifdef _WIN32
  if (error.category() == std::system_category()) {
    return error.value() == ERROR_SHARING_VIOLATION ||
           error.value() == ERROR_LOCK_VIOLATION ||
           error.value() == ERROR_CLOUD_FILE_IN_USE;
  }
#endif
  return error == std::errc::device_or_resource_busy;
}

QString local_content_revision(const std::filesystem::path &path,
                               const QString &expected_identity,
                               const QString &expected_revision) {
#ifdef _WIN32
  const auto data = find_data_for_path(path);
  if (!data || !can_read_local_content(data->dwFileAttributes, data->dwReserved0)) {
    return {};
  }
  // Deny writes and replacement during hashing. Recheck residency on the same
  // handle before reading, since the path could have changed after enumeration.
  const WinHandle handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OPEN_NO_RECALL |
      FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
  FILE_ATTRIBUTE_TAG_INFO tag{};
  BY_HANDLE_FILE_INFORMATION info{};
  if (!handle.valid() || GetFileType(handle.value) != FILE_TYPE_DISK ||
      !GetFileInformationByHandleEx(handle.value, FileAttributeTagInfo, &tag, sizeof(tag)) ||
      !can_read_local_content(tag.FileAttributes, tag.ReparseTag) ||
      !GetFileInformationByHandle(handle.value, &info)) {
    return {};
  }
  if (!expected_identity.isEmpty() && file_id_identity(info) != expected_identity) {
    return {};
  }
  if (!expected_revision.isEmpty() &&
      write_revision(info.ftLastWriteTime, info.nFileSizeHigh, info.nFileSizeLow) != expected_revision) {
    return {};
  }
  return hash_data_stream(handle.value, info);
#elif defined(__APPLE__)
  return macos_content_revision(path, expected_identity, expected_revision);
#else
  Q_UNUSED(path);
  Q_UNUSED(expected_identity);
  Q_UNUSED(expected_revision);
  return {};
#endif
}

QString move_lock_message() {
  return QStringLiteral("This item is in use. Retry after the application or sync client releases it.");
}

const PlatformProbe &default_platform_probe(SyncProvider provider) {
  static const DefaultPlatformProbe onedrive(SyncProvider::OneDrive);
  static const DefaultPlatformProbe dropbox(SyncProvider::Dropbox);
  return provider == SyncProvider::Dropbox ? dropbox : onedrive;
}

#ifdef _WIN32
std::error_code rename_windows_dropbox_path(const std::filesystem::path &source,
                                            const std::filesystem::path &destination) {
  if (MoveFileExW(source.c_str(), destination.c_str(), 0)) {
    return {};
  }
  return {static_cast<int>(GetLastError()), std::system_category()};
}
#endif

bool is_dropbox_metadata_name(const QString &name) {
  const auto lower = name.toCaseFolded();
  return lower == QStringLiteral(".dropbox") || lower == QStringLiteral(".dropbox.attr") ||
         lower == QStringLiteral(".dropbox.cache");
}

bool is_dropbox_metadata_path(const std::filesystem::path &path) {
  return std::any_of(path.begin(), path.end(), [](const auto &part) {
    return is_dropbox_metadata_name(path_to_qstring(part));
  });
}

SyncRootDetection detect_onedrive_root(const std::filesystem::path &path,
                                       const PlatformProbe &platform_probe) {
  const SyncRootDetection platform_detection = platform_probe.detect_sync_root(path);
#ifdef __APPLE__
  return platform_detection;
#else
  if (platform_detection.authoritative) {
    return platform_detection;
  }

  if (path_is_under_env_root(path)) {
    return SyncRootDetection{
        .matched = true,
        .authoritative = false,
        .vetoed = false,
        .confidence = 85,
        .source = QStringLiteral("onedrive_environment"),
        .provider_name = QStringLiteral("OneDrive"),
        .message = QStringLiteral("Detected OneDrive from environment roots.")};
  }

  if (path_has_onedrive_component(path)) {
    return SyncRootDetection{
        .matched = true,
        .authoritative = false,
        .vetoed = false,
        .confidence = 55,
        .source = QStringLiteral("path_heuristic"),
        .provider_name = QStringLiteral("OneDrive"),
        .message = QStringLiteral("Detected OneDrive from the folder path.")};
  }

  return {};
#endif
}

PathInspection inspect_onedrive_path(const std::filesystem::path &path,
                                     const PlatformProbe &platform_probe) {
  return inspect_sync_path(path, platform_probe, SyncProvider::OneDrive);
}

PathInspection inspect_sync_path(const std::filesystem::path &path,
                                 const PlatformProbe &platform_probe, SyncProvider provider) {
  auto inspection = platform_probe.inspect_path(path);
  auto &info = inspection.state;

  const bool dropbox = provider == SyncProvider::Dropbox;
  const auto provider_name = dropbox ? QStringLiteral("Dropbox") : QStringLiteral("OneDrive");
  const QString filename = path_to_qstring(path.filename());
  const bool conflict_name = dropbox ? filename.contains(QStringLiteral("conflicted copy"), Qt::CaseInsensitive)
                                    : filename_looks_like_conflict_copy(filename);
  info.conflict_copy = info.conflict_copy || conflict_name;
  const bool lock_name = dropbox ? filename_looks_like_office_lock(filename) : path_name_is_sync_locked(path);
  info.sync_locked = info.sync_locked || lock_name || info.conflict_copy;

  if (dropbox && is_dropbox_metadata_path(path)) {
    info.move_blocked = true;
    info.message = QStringLiteral("Dropbox client metadata cannot be moved.");
  }
  if (dropbox && info.hydration_required) {
    info.move_blocked = true;
  }
  const bool retry_sync_lock = info.sync_locked && !info.move_blocked;
  info.should_retry = !info.conflict_copy &&
                      (info.should_retry || retry_sync_lock || info.hydration_required);
  if (!info.sync_locked || !info.should_retry) {
    info.retry_after_ms = 0;
  } else if (info.retry_after_ms == 0) {
    info.retry_after_ms = kLockRetryAfterMs;
  }

  if (info.stable_identity.isEmpty()) {
    info.stable_identity = stable_path_identity(path);
  }
#if !defined(_WIN32) && !defined(__APPLE__)
  if (info.revision_suffix.isEmpty()) {
    info.revision_suffix = local_revision_suffix(inspection.size_bytes, inspection.mtime);
  }
#endif

  if (info.conflict_copy) {
    info.message = QStringLiteral("%1 conflict copy requires manual review before moving.").arg(provider_name);
  } else if (info.sync_locked && info.message.isEmpty()) {
    info.message = QStringLiteral("%1 is still syncing this item.").arg(provider_name);
  } else if (info.hydration_required && info.message.isEmpty()) {
    info.message = dropbox ? QStringLiteral("Make this Dropbox item available offline before moving it.")
                           : QStringLiteral("OneDrive has not fully hydrated this item yet.");
  }

  return inspection;
}

} // namespace one_drive
