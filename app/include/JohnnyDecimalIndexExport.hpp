#pragma once

#include <filesystem>
#include <string>

#include "JohnnyDecimalArchiveIndex.hpp"

namespace JohnnyDecimalIndexExport {

/**
 * @brief Atomically writes a complete archive snapshot as linked UTF-8 Markdown.
 * @param index Snapshot to export, independent of any UI search filter.
 * @param destination Output Markdown file; parent directory must already exist.
 * @param error Optional output for an actionable filesystem error.
 * @return True when the complete document was committed successfully.
 */
bool save(const JohnnyDecimalArchiveIndex::Index& index, const std::filesystem::path& destination,
          std::string* error = nullptr);

}  // namespace JohnnyDecimalIndexExport
