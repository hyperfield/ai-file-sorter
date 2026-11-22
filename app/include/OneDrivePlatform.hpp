#pragma once

#include <QString>

#include <filesystem>
#include <system_error>

namespace one_drive {

/** @brief Delay suggested after a temporary sharing or sync lock. */
inline constexpr int kLockRetryAfterMs = 2000;

/** @brief Filesystem traits used to filter an entry before scanning it. */
struct ScanEntryInfo {
  bool hidden{false};
  bool directory{false};
  bool regular_file{false};
};

struct SyncRootDetection {
  bool matched{false};
  bool authoritative{false};
  bool vetoed{false};
  int confidence{0};
  QString source;
  QString provider_name;
  QString message;
};

struct PlatformFileInfo {
  bool hydration_required{false};
  bool sync_locked{false};
  bool conflict_copy{false};
  bool should_retry{false};
  int retry_after_ms{0};
  bool move_blocked{false};
  bool can_capture_content_revision{false};
  QString stable_identity;
  QString revision_suffix;
  QString message;
};

struct PathInspection {
  bool exists{false};
  qint64 size_bytes{0};
  qint64 mtime{0};
  PlatformFileInfo state;
};

class PlatformProbe {
public:
  virtual ~PlatformProbe() = default;

  virtual SyncRootDetection
  detect_sync_root(const std::filesystem::path &path) const = 0;

  virtual PathInspection inspect_path(const std::filesystem::path &path) const;
};

std::filesystem::path path_from_qstring(const QString &value);
QString path_to_qstring(const std::filesystem::path &path);
QString normalized_path_text(const std::filesystem::path &path);

/**
 * @brief Reads entry traits without opening file contents or following links.
 * @param path Entry discovered by directory enumeration.
 * @return Traits for a normal or Cloud Files entry, or an unclassified entry for a link.
 * @throws std::filesystem::filesystem_error If entry metadata cannot be read.
 */
ScanEntryInfo scan_entry_info(const std::filesystem::path &path);

/**
 * @brief Checks whether a filesystem failure indicates a temporary move lock.
 * @param error Native filesystem error from an open or rename operation.
 * @return True for Windows sharing, byte-range, or Cloud Files use conflicts.
 */
bool is_retryable_move_error(const std::error_code &error);

/** @brief Returns the diagnostic shared by preflight and move lock failures. */
QString move_lock_message();

/**
 * @brief Computes a SHA-256 revision for a fully local regular file without recalling cloud data.
 * @param path File whose default data stream should be read.
 * @param expected_identity Native identity to bind a new snapshot to, or empty when verifying contents.
 * @param expected_revision Native revision suffix to bind a new snapshot to, or empty when verifying.
 * @return Content revision, or an empty string if a safe snapshot is unavailable.
 */
QString local_content_revision(const std::filesystem::path &path,
                               const QString &expected_identity,
                               const QString &expected_revision);

const PlatformProbe &default_platform_probe();

SyncRootDetection detect_onedrive_root(
    const std::filesystem::path &path,
    const PlatformProbe &platform_probe = default_platform_probe());
PathInspection inspect_onedrive_path(
    const std::filesystem::path &path,
    const PlatformProbe &platform_probe = default_platform_probe());

} // namespace one_drive
