#pragma once

#include "OneDrivePlatform.hpp"

namespace one_drive {

struct MacFinderFlags {
  bool hidden{false};
  bool alias{false};
};

/** Reads Finder flags from filesystem metadata without resolving aliases. */
MacFinderFlags read_macos_finder_flags(const std::filesystem::path &path,
                                      std::error_code &error);

ScanEntryInfo scan_macos_entry(const std::filesystem::path &path, bool dot_hidden);

} // namespace one_drive
