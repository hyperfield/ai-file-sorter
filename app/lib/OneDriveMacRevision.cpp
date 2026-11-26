#include "OneDriveMacRevision.hpp"

#include "MacDatalessFiles.hpp"

#include <QByteArray>
#include <QCryptographicHash>

#include <fcntl.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <algorithm>
#include <cerrno>
#include <chrono>

namespace one_drive {
namespace {

using Clock = std::chrono::steady_clock;
// Leave room below the host's five-second request timeout.
constexpr auto kHashBudget = std::chrono::milliseconds(2500);
constexpr std::size_t kHashBufferSize = 256 * 1024;
constexpr auto kContentRevisionPrefix = "onedrive:sha256:";

class ReadHandle {
public:
  explicit ReadHandle(int fd) : fd_(fd) {}
  ReadHandle(const ReadHandle &) = delete;
  ReadHandle &operator=(const ReadHandle &) = delete;
  ~ReadHandle() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }
  int get() const { return fd_; }

private:
  int fd_;
};

template <typename ReadAttributes>
QString volume_identifier(const struct stat &metadata, const ReadAttributes &read_attributes) {
  struct attrlist attrs {};
  attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
  attrs.volattr = ATTR_VOL_INFO | ATTR_VOL_UUID;
  struct {
    uint32_t length;
    uuid_t uuid;
  } data{};
  if (read_attributes(attrs, &data, sizeof(data), 0) == 0 &&
      data.length == sizeof(data) && !uuid_is_null(data.uuid)) {
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(data.uuid), sizeof(data.uuid)).toHex());
  }
  // Filesystems without a volume UUID get an identity for the current mount.
  return QStringLiteral("dev-%1").arg(static_cast<quint64>(metadata.st_dev));
}

template <typename ReadAttributes>
uint32_t generation_count(const ReadAttributes &read_attributes) {
  struct attrlist attrs {};
  attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
  attrs.commonattr = ATTR_CMN_GEN_COUNT;
  struct {
    uint32_t length;
    uint32_t value;
  } data{};
  if (read_attributes(attrs, &data, sizeof(data), FSOPT_ATTR_CMN_EXTENDED) == 0 &&
      data.length == sizeof(data)) {
    return data.value;
  }
  return 0;
}

QString time_token(const struct timespec &time) {
  return QStringLiteral("%1.%2").arg(time.tv_sec).arg(time.tv_nsec);
}

template <typename ReadAttributes>
PathInspection describe(const struct stat &metadata, const ReadAttributes &read_attributes) {
  const bool regular = S_ISREG(metadata.st_mode);
  PathInspection result{
      .exists = true,
      .size_bytes = regular ? metadata.st_size : 0,
      .mtime = metadata.st_mtimespec.tv_sec};
  auto &state = result.state;
  state.hydration_required = (metadata.st_flags & SF_DATALESS) != 0;
  if (!regular && !S_ISDIR(metadata.st_mode)) {
    return result;
  }
  const auto id = QStringLiteral("%1:%2")
                      .arg(volume_identifier(metadata, read_attributes))
                      .arg(static_cast<quint64>(metadata.st_ino));
  state.stable_identity = QStringLiteral("onedrive:mac-fileid:%1").arg(id);
  // Generation tracks changes to the data even if the write time is restored.
  // Change time is a conservative fallback on other filesystems.
  const auto generation = generation_count(read_attributes);
  const auto edit = generation != 0
      ? QStringLiteral("g%1").arg(generation)
      : QStringLiteral("c%1").arg(time_token(metadata.st_ctimespec));
  state.revision_suffix = QString::fromLatin1(kMacWriteRevisionPrefix) +
      QStringLiteral("%1:%2:%3:%4:%5").arg(id)
          .arg(metadata.st_size)
          .arg(time_token(metadata.st_birthtimespec), time_token(metadata.st_mtimespec), edit);
  state.can_capture_content_revision = regular && !state.hydration_required;
  return result;
}

bool same_time(const struct timespec &first, const struct timespec &second) {
  return first.tv_sec == second.tv_sec && first.tv_nsec == second.tv_nsec;
}

bool same_snapshot(const struct stat &first, const struct stat &second) {
  return first.st_dev == second.st_dev && first.st_ino == second.st_ino &&
         first.st_size == second.st_size &&
         same_time(first.st_birthtimespec, second.st_birthtimespec) &&
         same_time(first.st_mtimespec, second.st_mtimespec) &&
         same_time(first.st_ctimespec, second.st_ctimespec);
}

bool readable_local_file(const struct stat &metadata) {
  return S_ISREG(metadata.st_mode) && (metadata.st_flags & SF_DATALESS) == 0;
}

QString hash_bytes(int fd, off_t remaining, Clock::time_point deadline) {
  QByteArray buffer(kHashBufferSize, Qt::Uninitialized);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (remaining > 0) {
    if (Clock::now() >= deadline) {
      return {};
    }
    const auto count = static_cast<std::size_t>(std::min<off_t>(remaining, buffer.size()));
    const auto actual = read(fd, buffer.data(), count);
    if (actual < 0 && errno == EINTR) {
      continue;
    }
    if (actual <= 0) {
      return {};
    }
    hash.addData(QByteArrayView(buffer.constData(), actual));
    remaining -= actual;
  }
  return QString::fromLatin1(kContentRevisionPrefix) +
         QString::fromLatin1(hash.result().toHex());
}

PathInspection describe_descriptor(int fd, const struct stat &metadata) {
  return describe(metadata, [fd](struct attrlist &attrs, void *data,
                                 std::size_t size, unsigned long options) {
    return fgetattrlist(fd, &attrs, data, size, options);
  });
}

} // namespace

PathInspection macos_metadata_for_path(const std::filesystem::path &path,
                                      const struct stat &metadata) {
  return describe(metadata, [&path](struct attrlist &attrs, void *data,
                                    std::size_t size, unsigned long options) {
    return getattrlist(path.c_str(), &attrs, data, size, options | FSOPT_NOFOLLOW);
  });
}

PathInspection macos_metadata_for_descriptor(int descriptor) {
  struct stat metadata {};
  if (fstat(descriptor, &metadata) != 0) {
    // Keep a token that can never match a later inspection if the moved file's
    // metadata became unavailable. An empty token would leave undo unguarded.
    PathInspection unavailable;
    unavailable.state.revision_suffix = QString::fromLatin1(kMacUnavailableRevision);
    return unavailable;
  }
  return describe_descriptor(descriptor, metadata);
}

QString macos_content_revision(const std::filesystem::path &path,
                               const QString &expected_identity,
                               const QString &expected_revision) {
  const auto deadline = Clock::now() + kHashBudget;
  const mac_dataless::DisableMaterialization no_downloads;
  struct stat initial {};
  if (!no_downloads.active() || path.empty() ||
      path.native().find('\0') != std::filesystem::path::string_type::npos ||
      lstat(path.c_str(), &initial) != 0 || !readable_local_file(initial)) {
    return {};
  }

  const ReadHandle handle(open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
  struct stat before {};
  struct statfs filesystem {};
  if (handle.get() < 0 || fstat(handle.get(), &before) != 0 ||
      !readable_local_file(before) || !same_snapshot(initial, before) ||
      fstatfs(handle.get(), &filesystem) != 0 || (filesystem.f_flags & MNT_LOCAL) == 0) {
    return {};
  }

  const auto snapshot = describe_descriptor(handle.get(), before);
  if ((!expected_identity.isEmpty() && expected_identity != snapshot.state.stable_identity) ||
      (!expected_revision.isEmpty() && expected_revision != snapshot.state.revision_suffix)) {
    return {};
  }

  const auto hash = hash_bytes(handle.get(), before.st_size, deadline);
  struct stat after {}, current {};
  if (hash.isEmpty() || Clock::now() >= deadline ||
      fstat(handle.get(), &after) != 0 || lstat(path.c_str(), &current) != 0 ||
      !readable_local_file(after) || !readable_local_file(current) ||
      !same_snapshot(before, after) || !same_snapshot(after, current) ||
      describe_descriptor(handle.get(), after).state.revision_suffix != snapshot.state.revision_suffix) {
    return {};
  }
  return hash;
}

} // namespace one_drive
