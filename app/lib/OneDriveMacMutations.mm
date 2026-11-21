#include "OneDriveMacMutations.hpp"

#include "OneDriveMacMaterialization.hpp"
#include "OneDriveMacPathInspection.hpp"
#include "OneDriveMacPaths.hpp"
#include "OneDriveMacRevision.hpp"

#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <system_error>

namespace one_drive {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

// Leave time for the connector to report the result before the host kills it.
constexpr auto kCoordinationBudget = std::chrono::milliseconds(2500);
constexpr int kDirectoryOpenFlags = O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW_ANY;

class FileHandle {
public:
  explicit FileHandle(int fd) : fd_(fd) {}
  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;
  ~FileHandle() { reset(-1); }

  int get() const { return fd_; }
  void reset(int fd) {
    if (fd_ >= 0) {
      close(fd_);
    }
    fd_ = fd;
  }

private:
  int fd_;
};

MacMutationResult native_failure(const QString &code, int error) {
  switch (error) {
  case EEXIST:
  case ENOTEMPTY:
    return {code, QStringLiteral("Destination path already exists."), true};
  case EXDEV:
    return {code, QStringLiteral("OneDrive moves on macOS require the same volume."), true};
  case ENODEV:
    return {code, QStringLiteral("The destination volume is unavailable. Reconnect it and retry."), true, true};
  case EDEADLK:
    return {code, QStringLiteral("Download the item or containing folder in Finder before moving it."), true};
  case EACCES:
  case EPERM:
  case EROFS:
    return {code, QStringLiteral("macOS denied access to change this path: %1")
                      .arg(QString::fromStdString(std::generic_category().message(error)))};
  case ENOTSUP:
    return {code, QStringLiteral("This filesystem does not support a move that protects existing destinations.")};
  default:
    break;
  }
  const bool retry = is_retryable_macos_error(error);
  return {code, QString::fromStdString(std::generic_category().message(error)), retry, retry};
}

MacMutationResult timed_out() {
  return {QStringLiteral("coordination_timeout"),
          QStringLiteral("Another process is using this location. Try again shortly."), true, true};
}

MacMutationResult coordination_failure(NSError *error) {
  NSString *domain = error.domain;
  if ([domain isEqualToString:NSCocoaErrorDomain]) {
    switch (error.code) {
    case NSFileWriteFileExistsError:
      return native_failure(QStringLiteral("coordination_failed"), EEXIST);
    case NSFileWriteNoPermissionError:
    case NSFileReadNoPermissionError:
      return native_failure(QStringLiteral("coordination_failed"), EACCES);
    case NSFileWriteVolumeReadOnlyError:
      return native_failure(QStringLiteral("coordination_failed"), EROFS);
    default:
      break;
    }
  }
  NSError *posix = [domain isEqualToString:NSPOSIXErrorDomain]
                     ? error : error.userInfo[NSUnderlyingErrorKey];
  if ([posix.domain isEqualToString:NSPOSIXErrorDomain]) {
    return native_failure(QStringLiteral("coordination_failed"), static_cast<int>(posix.code));
  }
  return {QStringLiteral("coordination_failed"),
          error ? QString::fromNSString(error.localizedDescription)
                : QStringLiteral("macOS could not coordinate this operation.")};
}

NSURL *file_url(const fs::path &path, bool is_directory) {
  return [NSURL fileURLWithFileSystemRepresentation:path.c_str()
                                        isDirectory:is_directory
                                      relativeToURL:nil];
}

bool valid_path(const fs::path &path) {
  return !path.empty() && path.native().find('\0') == fs::path::string_type::npos;
}

bool same_file(const struct stat &first, const struct stat &second) {
  return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

bool agrees_with_path(NSURL *url, fs::path expected) {
  const char *native_path = url.fileSystemRepresentation;
  if (!native_path) {
    return false;
  }
  fs::path returned(native_path);
  if (returned.has_relative_path() && !returned.has_filename()) {
    returned = returned.parent_path();
  }

  // Exact spellings for the portions that may not exist, then let the
  // filesystem decide whether the existing ancestor spellings are equivalent.
  while (returned != expected && returned.has_relative_path() &&
         expected.has_relative_path() && returned.filename() == expected.filename()) {
    returned = returned.parent_path();
    expected = expected.parent_path();
  }
  if (returned == expected) {
    return true;
  }
  struct stat left {}, right {};
  return lstat(returned.c_str(), &left) == 0 &&
         lstat(expected.c_str(), &right) == 0 &&
         same_file(left, right) &&
         (S_ISDIR(left.st_mode) || (S_ISREG(left.st_mode) && left.st_nlink == 1));
}

bool remove_empty_directory(const fs::path &path, const struct stat &expected) {
  const FileHandle parent(open(path.parent_path().c_str(), kDirectoryOpenFlags));
  if (parent.get() < 0) {
    return false;
  }
  struct stat current {};
  if (fstatat(parent.get(), path.filename().c_str(), &current, AT_SYMLINK_NOFOLLOW) != 0 ||
      !same_file(current, expected) || (current.st_flags & SF_DATALESS) != 0) {
    return false;
  }
  return unlinkat(parent.get(), path.filename().c_str(), AT_REMOVEDIR) == 0;
}

// A failed operation should only remove empty folders that it actually created.
class CreatedDirectories {
public:
  ~CreatedDirectories() {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
      remove_empty_directory(it->first, it->second);
    }
  }

  void prepare(std::size_t size) { entries_.reserve(size); }
  void record(fs::path path, const struct stat &identity) {
    entries_.emplace_back(std::move(path), identity);
  }
  void keep() { entries_.clear(); }

private:
  std::vector<std::pair<fs::path, struct stat>> entries_;
};

struct DirectoryPlan {
  fs::path target;
  fs::path existing;
  std::vector<fs::path> missing;
  struct stat anchor {};
};

std::error_code plan_directory(const fs::path &path, DirectoryPlan &plan) {
  std::error_code ec;
  const auto absolute = fs::absolute(path, ec);
  if (ec) {
    return ec;
  }
  plan.target = fs::weakly_canonical(absolute, ec);
  if (ec) {
    return ec;
  }
  if (plan.target.has_relative_path() && !plan.target.has_filename()) {
    plan.target = plan.target.parent_path();
  }
  plan.existing = plan.target;
  while (lstat(plan.existing.c_str(), &plan.anchor) != 0) {
    if (errno != ENOENT || plan.existing == plan.existing.root_path()) {
      return {errno, std::generic_category()};
    }
    plan.missing.push_back(plan.existing.filename());
    plan.existing = plan.existing.parent_path();
  }
  // A disconnected volume must not turn into a directory on the boot disk.
  if (plan.existing == fs::path("/Volumes") && !plan.missing.empty()) {
    return {ENODEV, std::generic_category()};
  }
  if (!S_ISDIR(plan.anchor.st_mode)) {
    return std::make_error_code(std::errc::not_a_directory);
  }
  if ((plan.anchor.st_flags & SF_DATALESS) != 0) {
    return {EDEADLK, std::generic_category()};
  }
  return {};
}

std::error_code create_directories(const DirectoryPlan &plan, FileHandle &parent,
                                   CreatedDirectories &created, Clock::time_point deadline) {
  parent.reset(open(plan.existing.c_str(), kDirectoryOpenFlags));
  struct stat anchor {};
  if (parent.get() < 0 || fstat(parent.get(), &anchor) != 0) {
    return {errno, std::generic_category()};
  }
  if (!same_file(anchor, plan.anchor)) {
    return {ESTALE, std::generic_category()};
  }
  if ((anchor.st_flags & SF_DATALESS) != 0) {
    return {EDEADLK, std::generic_category()};
  }

  auto path = plan.existing;
  created.prepare(plan.missing.size());
  for (auto it = plan.missing.rbegin(); it != plan.missing.rend(); ++it) {
    if (Clock::now() >= deadline) {
      return {ETIMEDOUT, std::generic_category()};
    }
    path /= *it;
    const bool made = mkdirat(parent.get(), it->c_str(), 0777) == 0;
    if (!made && errno != EEXIST) {
      return {errno, std::generic_category()};
    }
    struct stat child {};
    if (fstatat(parent.get(), it->c_str(), &child, AT_SYMLINK_NOFOLLOW) != 0) {
      return {errno, std::generic_category()};
    }
    if (!S_ISDIR(child.st_mode)) {
      return {ENOTDIR, std::generic_category()};
    }
    if (made) {
      created.record(path, child);
    }
    if ((child.st_flags & SF_DATALESS) != 0) {
      return {EDEADLK, std::generic_category()};
    }
    parent.reset(openat(parent.get(), it->c_str(), kDirectoryOpenFlags));
    if (parent.get() < 0) {
      return {errno, std::generic_category()};
    }
  }
  return {};
}

dispatch_source_t cancel_at(NSFileCoordinator *coordinator, Clock::time_point deadline) {
  auto timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                      dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
  const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
      deadline - Clock::now()).count();
  dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, std::max<int64_t>(0, remaining)),
                            DISPATCH_TIME_FOREVER, 0);
  dispatch_source_set_event_handler(timer, ^{ [coordinator cancel]; });
  dispatch_resume(timer);
  return timer;
}

void clean_undo_directories(const fs::path &source_parent,
                            const std::vector<fs::path> &recorded,
                            Clock::time_point deadline) {
  std::vector<struct stat> verified;
  for (const auto &path : recorded) {
    if (Clock::now() >= deadline) {
      return;
    }
    struct stat metadata {};
    if (valid_path(path) && lstat(path.c_str(), &metadata) == 0 &&
        S_ISDIR(metadata.st_mode)) {
      verified.push_back(metadata);
    }
  }

  for (auto parent = source_parent; parent != parent.root_path(); parent = parent.parent_path()) {
    struct stat expected {};
    if (Clock::now() >= deadline || lstat(parent.c_str(), &expected) != 0 ||
        !S_ISDIR(expected.st_mode) || (expected.st_flags & SF_DATALESS) != 0 ||
        std::none_of(verified.begin(), verified.end(),
                     [&](const auto &entry) { return same_file(expected, entry); })) {
      break;
    }
    NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
    auto timer = cancel_at(coordinator, deadline);
    __block bool removed = false;
    [coordinator coordinateWritingItemAtURL:file_url(parent, true)
                                    options:NSFileCoordinatorWritingForDeleting
                                      error:nil
                                 byAccessor:^(NSURL *url) {
      if (Clock::now() < deadline && agrees_with_path(url, parent)) {
        removed = remove_empty_directory(parent, expected);
      }
    }];
    dispatch_source_cancel(timer);
    if (!removed) {
      break;
    }
  }
}

} // namespace

MacMutationResult ensure_macos_directory(const fs::path &directory) {
  @autoreleasepool {
    const DisableMaterialization no_downloads;
    const auto code = QStringLiteral("ensure_directory_failed");
    if (!no_downloads.active() || !valid_path(directory)) {
      return native_failure(code, EINVAL);
    }

    DirectoryPlan plan;
    if (const auto error = plan_directory(directory, plan)) {
      return native_failure(code, error.value());
    }
    if (plan.missing.empty()) {
      return {};
    }

    const auto deadline = Clock::now() + kCoordinationBudget;
    NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
    auto timer = cancel_at(coordinator, deadline);
    NSError *error = nil;
    __block auto result = timed_out();
    __block bool accessed = false;
    [coordinator coordinateWritingItemAtURL:file_url(plan.target, true)
                                    options:0
                                      error:&error
                                 byAccessor:^(NSURL *url) {
      accessed = true;
      if (!agrees_with_path(url, plan.target)) {
        result = native_failure(code, ESTALE);
      } else if (Clock::now() < deadline) {
        FileHandle parent(-1);
        CreatedDirectories created;
        const auto create_error = create_directories(plan, parent, created, deadline);
        result = create_error ? native_failure(code, create_error.value()) : MacMutationResult{};
        if (!create_error) {
          created.keep();
        }
      }
    }];
    dispatch_source_cancel(timer);
    return accessed || Clock::now() >= deadline ? result : coordination_failure(error);
  }
}

MacMutationResult move_macos_path(const fs::path &source, const fs::path &destination,
                                 const std::vector<fs::path> &undo_directories) {
  @autoreleasepool {
    const DisableMaterialization no_downloads;
    const auto code = QStringLiteral("move_failed");
    if (!no_downloads.active() || !valid_path(source) || !valid_path(destination) ||
        !source.has_filename() || !destination.has_filename() ||
        source.filename() == "." || source.filename() == ".." ||
        destination.filename() == "." || destination.filename() == "..") {
      return native_failure(code, EINVAL);
    }
    const auto deadline = Clock::now() + kCoordinationBudget;
    std::error_code ec;
    const auto source_path = fs::absolute(source, ec);
    if (ec) {
      return native_failure(code, ec.value());
    }
    const auto source_parent = fs::canonical(source_path.parent_path(), ec);
    if (ec) {
      return native_failure(code, ec.value());
    }
    const auto from = source_parent / source.filename();
    struct stat original {};
    if (lstat(from.c_str(), &original) != 0) {
      return native_failure(code, errno);
    }
    if (!S_ISREG(original.st_mode) && !S_ISDIR(original.st_mode)) {
      return {code, QStringLiteral("Only regular files and folders can be moved on macOS."), true};
    }
    if ((original.st_flags & SF_DATALESS) != 0) {
      return native_failure(code, EDEADLK);
    }
    DirectoryPlan plan;
    const auto parent_path = destination.has_parent_path() ? destination.parent_path() : fs::path(".");
    const auto create_code = QStringLiteral("create_destination_failed");
    if (const auto error = plan_directory(parent_path, plan)) {
      return native_failure(create_code, error.value());
    }
    if (original.st_dev != plan.anchor.st_dev) {
      return native_failure(code, EXDEV);
    }
    const auto to = plan.target / destination.filename();

    NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
    auto timer = cancel_at(coordinator, deadline);
    NSError *error = nil;
    __block bool accessed = false;
    __block auto result = timed_out();
    [coordinator coordinateWritingItemAtURL:file_url(from, S_ISDIR(original.st_mode))
                                    options:NSFileCoordinatorWritingForMoving
                           writingItemAtURL:file_url(to, S_ISDIR(original.st_mode))
                                    options:NSFileCoordinatorWritingForReplacing
                                      error:&error
                                 byAccessor:^(NSURL *from_url, NSURL *to_url) {
      accessed = true;
      if (Clock::now() >= deadline) {
        return;
      }
      if (!agrees_with_path(from_url, from) || !agrees_with_path(to_url, to)) {
        result = native_failure(code, ESTALE);
        return;
      }

      const FileHandle old_parent(open(source_parent.c_str(), kDirectoryOpenFlags));
      if (old_parent.get() < 0) {
        result = native_failure(code, errno);
        return;
      }
      const FileHandle item(openat(old_parent.get(), from.filename().c_str(),
                                   O_EVTONLY | O_NOFOLLOW | O_CLOEXEC));
      struct stat current {};
      if (item.get() < 0 || fstat(item.get(), &current) != 0) {
        result = native_failure(code, errno);
        return;
      }
      if (!same_file(original, current)) {
        result = native_failure(code, ESTALE);
        return;
      }
      if ((current.st_flags & SF_DATALESS) != 0) {
        result = native_failure(code, EDEADLK);
        return;
      }

      FileHandle new_parent(-1);
      CreatedDirectories created;
      if (const auto create_error = create_directories(plan, new_parent, created, deadline)) {
        result = native_failure(create_code, create_error.value());
        return;
      }
      if (Clock::now() >= deadline) {
        return;
      }
      struct stat named_source {};
      if (fstatat(old_parent.get(), from.filename().c_str(),
                  &named_source, AT_SYMLINK_NOFOLLOW) != 0) {
        result = native_failure(code, errno);
        return;
      }
      if (!same_file(current, named_source)) {
        result = native_failure(code, ESTALE);
        return;
      }
      if ((named_source.st_flags & SF_DATALESS) != 0) {
        result = native_failure(code, EDEADLK);
        return;
      }
      if (S_ISREG(named_source.st_mode)) {
        std::error_code finder_error;
        const auto finder = read_macos_finder_flags(from, finder_error);
        if (finder.alias || finder_error) {
          result = finder_error
              ? native_failure(code, finder_error.value())
              : MacMutationResult{code, QStringLiteral("Finder aliases cannot be moved by this connector."), true};
          return;
        }
      }
      // Bind both parents and refuse replacement at the syscall, even if another
      // writer creates the destination after preflight or while coordinating.
      if (renameatx_np(old_parent.get(), from.filename().c_str(),
                       new_parent.get(), to.filename().c_str(), RENAME_EXCL) != 0) {
        result = native_failure(code, errno);
        return;
      }
      created.keep();
      result = {};
      // Capture metadata while the coordination is held, from the original
      // inode rather than whichever file happens to be at the destination later.
      auto metadata = macos_metadata_for_descriptor(item.get());
      struct stat named_destination {};
      if (fstatat(new_parent.get(), to.filename().c_str(),
                  &named_destination, AT_SYMLINK_NOFOLLOW) != 0 ||
          !same_file(current, named_destination)) {
        metadata.state.revision_suffix = QString::fromLatin1(kMacUnavailableRevision);
        metadata.state.can_capture_content_revision = false;
      }
      result.moved_metadata = std::move(metadata);
      // The helper is unsandboxed. Only the post-move presenter notification
      // is needed. The pre-move sandbox extension request does not apply.
      [coordinator itemAtURL:from_url didMoveToURL:to_url];
    }];
    dispatch_source_cancel(timer);

    if (!accessed && Clock::now() < deadline) {
      return coordination_failure(error);
    }
    if (result.error_code.isEmpty() && !undo_directories.empty()) {
      clean_undo_directories(source_parent, undo_directories, deadline);
    }
    return result;
  }
}

} // namespace one_drive
