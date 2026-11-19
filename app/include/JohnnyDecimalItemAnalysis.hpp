#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "JohnnyDecimalArchiveIndex.hpp"

namespace JohnnyDecimalItemAnalysis {

/** @brief Parsed standard AC.ID folder name, retaining the ID even when its title is missing. */
struct Name {
    std::string id;
    std::string label;
    bool numbered{false};
    bool valid{false};
};

/** @brief Item-level findings shared by validation and the navigable archive index. */
struct Item {
    std::string id;
    std::string relative_path;
    std::string category_id;
    bool missing_id{false};
    bool malformed_id{false};
    bool wrong_category{false};
    bool duplicate_id{false};
};

/**
 * @brief Parses a standard two-digit category and two-digit item ID followed by a title.
 * @param name Folder basename; titles may contain Unicode.
 * @return Parsed ID/title and syntax status, without numeric conversions or overflow.
 */
Name parse_name(std::string_view name);

/**
 * @brief Checks direct item folders inside recognized categories across one archive.
 * @param index Snapshot of the archive; ordinary subfolders inside items are not ID folders.
 * @return Findings for each direct item, including all occurrences of duplicate IDs.
 */
std::vector<Item> analyze(const JohnnyDecimalArchiveIndex::Index& index);

}  // namespace JohnnyDecimalItemAnalysis
