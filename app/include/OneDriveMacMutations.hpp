#pragma once

#include "OneDrivePlatform.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace one_drive {

struct MacMutationResult {
  QString error_code;
  QString message;
  bool skipped{false};
  bool retry{false};
  std::optional<PathInspection> moved_metadata;
};

/** Coordinates a same-volume move and, on undo, removes recorded empty parents. */
MacMutationResult move_macos_path(
    const std::filesystem::path &source,
    const std::filesystem::path &destination,
    const std::vector<std::filesystem::path> &undo_directories);

/** Creates missing destination folders under a coordinated write. */
MacMutationResult ensure_macos_directory(const std::filesystem::path &directory);

} // namespace one_drive
