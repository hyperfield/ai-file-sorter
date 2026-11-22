#pragma once

#include "OneDrivePlatform.hpp"

#include <sys/stat.h>

namespace one_drive {

inline constexpr auto kMacWriteRevisionPrefix = "mac-write-v1:";
inline constexpr auto kMacUnavailableRevision = "mac-unavailable-v1";

/** Reads native identity and revision metadata without opening the file's data. */
PathInspection macos_metadata_for_path(const std::filesystem::path &path,
                                      const struct stat &metadata);

/** Reads metadata from the entry held open across a coordinated move. */
PathInspection macos_metadata_for_descriptor(int descriptor);

/** Hashes a local regular file, optionally bound to the move's native metadata. */
QString macos_content_revision(const std::filesystem::path &path,
                               const QString &expected_identity,
                               const QString &expected_revision);

} // namespace one_drive
