#include "OneDriveMacPaths.hpp"

#include <sys/stat.h>
#include <sys/xattr.h>

#include <array>
#include <cerrno>

namespace one_drive {

MacFinderFlags read_macos_finder_flags(const std::filesystem::path &path,
                                      std::error_code &error) {
  // FinderInfo stores a big-endian flags word at offset 8 for both files and
  // directories. Reading the attribute does not open the file or its target.
  constexpr std::size_t kFlagsOffset = 8;
  constexpr unsigned kFinderInvisible = 0x4000;
  constexpr unsigned kFinderAlias = 0x8000;
  std::array<unsigned char, 32> finder_info{};
  const auto size = getxattr(path.c_str(), "com.apple.FinderInfo",
                             finder_info.data(), finder_info.size(), 0,
                             XATTR_NOFOLLOW);
  if (size < 0) {
    if (errno != ENOATTR && errno != ENOTSUP) {
      error = {errno, std::generic_category()};
    }
    return {};
  }
  if (static_cast<std::size_t>(size) != finder_info.size()) {
    error = std::make_error_code(std::errc::io_error);
    return {};
  }

  const unsigned flags = (static_cast<unsigned>(finder_info[kFlagsOffset]) << 8) |
                         finder_info[kFlagsOffset + 1];
  return {.hidden = (flags & kFinderInvisible) != 0,
          .alias = (flags & kFinderAlias) != 0};
}

ScanEntryInfo scan_macos_entry(const std::filesystem::path &path, bool dot_hidden) {
  struct stat metadata {};
  if (lstat(path.c_str(), &metadata) != 0) {
    throw std::filesystem::filesystem_error("Read directory entry", path,
                                           {errno, std::generic_category()});
  }
  const bool directory = S_ISDIR(metadata.st_mode);
  const bool regular = S_ISREG(metadata.st_mode);
  if (!directory && !regular) {
    return {};
  }

  std::error_code error;
  const auto finder = read_macos_finder_flags(path, error);
  if (error) {
    throw std::filesystem::filesystem_error("Read Finder metadata", path, error);
  }
  const bool hidden = dot_hidden || (metadata.st_flags & UF_HIDDEN) != 0 || finder.hidden;
  return {hidden, directory, regular && !finder.alias};
}

} // namespace one_drive
