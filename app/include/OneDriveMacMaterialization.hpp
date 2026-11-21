#pragma once

#include <sys/resource.h>

namespace one_drive {

inline constexpr int kMaterializationPolicy = IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES;
inline constexpr int kPolicyScope = IOPOL_SCOPE_THREAD;

// See Apple TN3150. Even stat can materialize intermediate folders unless
// materialization is disabled on the calling thread.
class DisableMaterialization {
public:
  DisableMaterialization()
      : previous_(getiopolicy_np(kMaterializationPolicy, kPolicyScope)),
        changed_(previous_ >= 0 && previous_ != IOPOL_MATERIALIZE_DATALESS_FILES_OFF &&
                 setiopolicy_np(kMaterializationPolicy, kPolicyScope,
                                IOPOL_MATERIALIZE_DATALESS_FILES_OFF) == 0) {}
  DisableMaterialization(const DisableMaterialization &) = delete;
  DisableMaterialization &operator=(const DisableMaterialization &) = delete;

  ~DisableMaterialization() {
    if (changed_) {
      setiopolicy_np(kMaterializationPolicy, kPolicyScope, previous_);
    }
  }

  bool active() const {
    return changed_ || previous_ == IOPOL_MATERIALIZE_DATALESS_FILES_OFF;
  }

private:
  int previous_;
  bool changed_;
};

} // namespace one_drive
