#include "OneDriveMacPathInspection.hpp"

#include "OneDriveMacMaterialization.hpp"
#include "OneDriveMacPaths.hpp"
#include "OneDriveMacRevision.hpp"

#include <sys/stat.h>
#include <sys/xattr.h>

#include <cerrno>
#include <system_error>

namespace one_drive {

bool is_retryable_macos_error(int error) {
  switch (error) {
  case EAGAIN:
  case EBUSY:
  case EINTR:
  case ESTALE:
  case ETIMEDOUT:
  case ENETDOWN:
  case ENETUNREACH:
  case EHOSTDOWN:
  case EHOSTUNREACH:
  case ENOTCONN:
    return true;
  default:
    return false;
  }
}

namespace {

PlatformFileInfo lookup_failure(int error) {
  if (error == ENOENT) {
    return {};
  }
  if (error == EDEADLK) {
    return {.hydration_required = true,
            .move_blocked = true,
            .message = QStringLiteral("Download the containing folder in Finder before moving this item.")};
  }

  const bool retryable = is_retryable_macos_error(error);
  return {.sync_locked = retryable,
          .should_retry = retryable,
          .retry_after_ms = retryable ? kLockRetryAfterMs : 0,
          .move_blocked = true,
          .message = QStringLiteral("Could not inspect this path: %1")
                         .arg(QString::fromStdString(std::generic_category().message(error)))};
}

bool has_filename_collision(const std::filesystem::path &path) {
  // File Provider records the original when it assigns a different local name
  // after a collision (documented in NSFileProviderManager.h).
  return getxattr(path.c_str(), "com.apple.fileprovider.before-bounce#PX",
                  nullptr, 0, 0, XATTR_NOFOLLOW) > 0;
}

} // namespace

PathInspection inspect_macos_path(const std::filesystem::path &path) {
  PathInspection inspection;
  if (path.empty() || path.native().find('\0') != std::filesystem::path::string_type::npos) {
    inspection.state = lookup_failure(EINVAL);
    return inspection;
  }

  const DisableMaterialization policy;
  if (!policy.active()) {
    inspection.state.move_blocked = true;
    inspection.state.message =
        QStringLiteral("Could not disable automatic downloads while inspecting this path.");
    return inspection;
  }

  struct stat metadata {};
  if (lstat(path.c_str(), &metadata) != 0) {
    inspection.state = lookup_failure(errno);
    return inspection;
  }

  inspection = macos_metadata_for_path(path, metadata);
  auto &state = inspection.state;
  if (state.hydration_required) {
    state.move_blocked = true;
    state.message = QStringLiteral("This OneDrive item is online-only. Download it in Finder before moving it.");
  }

  constexpr auto kMoveLockFlags = UF_IMMUTABLE | SF_IMMUTABLE | UF_APPEND |
                                   SF_APPEND | SF_NOUNLINK;
  if ((metadata.st_flags & kMoveLockFlags) != 0) {
    state.sync_locked = true;
    state.move_blocked = true;
    state.message = QStringLiteral("macOS has locked this item against moves.");
  }

  // The selected root may be a symlink. A link discovered as an item is
  // different: it is outside the connector's move and undo workflow.
  if (S_ISLNK(metadata.st_mode)) {
    state.move_blocked = true;
    state.message = QStringLiteral("Symbolic links cannot be moved by this connector.");
    return inspection;
  }
  if (S_ISREG(metadata.st_mode)) {
    std::error_code error;
    const auto finder = read_macos_finder_flags(path, error);
    if (error && !state.hydration_required) {
      state = lookup_failure(error.value());
      return inspection;
    }
    if (finder.alias) {
      state.move_blocked = true;
      state.message = QStringLiteral("Finder aliases cannot be moved by this connector.");
      return inspection;
    }
  }
  if (has_filename_collision(path)) {
    state.conflict_copy = true;
    state.move_blocked = true;
  }
  return inspection;
}

} // namespace one_drive
