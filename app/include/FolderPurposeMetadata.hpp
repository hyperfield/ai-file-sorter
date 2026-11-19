#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

/** @brief Bounded, optional archive-local descriptions of destination folder intent. */
namespace FolderPurposeMetadata {
/** @brief Documented archive-relative metadata filename; never a routing destination. */
inline constexpr char kFileName[] = ".aifs-folder-purposes.json";
/** @brief A complete metadata read, with an explicit diagnostic instead of partial invalid data. */
struct Result {
    std::map<std::string, std::string> purposes;
    std::string error;
};
/**
 * @brief Parse versioned folder descriptions without interpreting their text as instructions.
 * @param document UTF-8 JSON containing version:1 and a folders object of path-to-description strings.
 * @return Valid descriptions, or an error and no descriptions when validation fails.
 */
Result parse(std::string_view document);
/**
 * @brief Read optional metadata without following a metadata-file symlink or reading an unbounded file.
 * @param root Destination archive containing the metadata file.
 * @return Descriptions, an empty successful result if absent, or an explicit read/validation error.
 */
Result load(const std::filesystem::path& root);
}
