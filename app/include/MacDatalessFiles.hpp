#pragma once

#include <sys/resource.h>
#include <sys/stat.h>

#include <cerrno>
#include <filesystem>
#include <system_error>

namespace mac_dataless {

inline constexpr int kMaterializationPolicy = IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES;
inline constexpr int kPolicyScope = IOPOL_SCOPE_THREAD;

/** @brief Disables automatic File Provider materialization on the calling thread. */
class DisableMaterialization {
public:
  /** @brief Disables materialization and remembers the previous thread policy. */
  DisableMaterialization()
      : previous_(getiopolicy_np(kMaterializationPolicy, kPolicyScope)),
        changed_(previous_ >= 0 && previous_ != IOPOL_MATERIALIZE_DATALESS_FILES_OFF &&
                 setiopolicy_np(kMaterializationPolicy, kPolicyScope,
                                IOPOL_MATERIALIZE_DATALESS_FILES_OFF) == 0) {}
  DisableMaterialization(const DisableMaterialization &) = delete;
  DisableMaterialization &operator=(const DisableMaterialization &) = delete;

  /** @brief Restores the previous thread policy if this guard changed it. */
  ~DisableMaterialization() {
    if (changed_) {
      setiopolicy_np(kMaterializationPolicy, kPolicyScope, previous_);
    }
  }

  /** @return Whether automatic materialization is disabled. */
  bool active() const {
    return changed_ || previous_ == IOPOL_MATERIALIZE_DATALESS_FILES_OFF;
  }

private:
  int previous_;
  bool changed_;
};

/** @return Whether the stat metadata describes an entry with no local data. */
inline bool is_dataless(const struct stat& metadata) {
  return (metadata.st_flags & SF_DATALESS) != 0;
}

/** @return Whether macOS refused access because automatic materialization is disabled. */
inline bool materialization_blocked(const std::error_code& error) {
  return error == std::errc::resource_deadlock_would_occur;
}

/**
 * @brief Checks a path and its final symlink target for dataless data.
 * @param path Path whose contents would be accessed. Use with materialization disabled.
 * @return True if the entry is dataless or an intermediate dataless directory blocked the lookup.
 */
inline bool needs_materialization(const std::filesystem::path& path) {
  struct stat metadata {};
  if (stat(path.c_str(), &metadata) == 0) {
    return is_dataless(metadata);
  }
  return errno == EDEADLK;
}

} // namespace mac_dataless
