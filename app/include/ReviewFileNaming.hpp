#pragma once

#include "Types.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ReviewFileNaming {

/**
 * @brief Builds a case-insensitive key for comparing review filenames.
 * @param value Filename or path segment to normalize.
 * @return Lowercase comparison key.
 */
std::string to_lower_copy_str(std::string value);

/**
 * @brief Returns whether a review entry is an image type supported by rename workflows.
 * @param file_path Directory containing the file.
 * @param file_name File name to inspect.
 * @param file_type Entry type.
 * @return True when the entry is a supported image file.
 */
bool is_supported_image_entry(const std::string& file_path,
                              const std::string& file_name,
                              FileType file_type);

/**
 * @brief Returns whether a review entry is a document type supported by rename workflows.
 * @param file_path Directory containing the file.
 * @param file_name File name to inspect.
 * @param file_type Entry type.
 * @return True when the entry is a supported document file.
 */
bool is_supported_document_entry(const std::string& file_path,
                                 const std::string& file_name,
                                 FileType file_type);

/**
 * @brief Resolves the target directory for a reviewed file.
 * @param file Reviewed categorization entry.
 * @param base_dir_override Base folder for category moves, or empty to use the file source directory.
 * @param use_subcategory True to include subcategory folders.
 * @param move_categorized_entries True when categorized entries move into category folders.
 * @return Destination directory for the suggested operation.
 */
std::filesystem::path build_suggested_target_dir(const CategorizedFile& file,
                                                 const std::string& base_dir_override,
                                                 bool use_subcategory,
                                                 bool move_categorized_entries = true);

/**
 * @brief Builds a unique rename suggestion with underscore numeric suffixes.
 * @param desired_name Desired suggested filename.
 * @param target_dir Directory where the filename must be unique.
 * @param used_names Case-insensitive names already allocated by the current review pass.
 * @param next_index Next suffix index per base filename key.
 * @param force_numbering True to number even the first duplicate suggestion.
 * @return A filename that avoids current review and on-disk collisions.
 */
std::string build_unique_suggested_name(const std::string& desired_name,
                                        const std::filesystem::path& target_dir,
                                        std::unordered_set<std::string>& used_names,
                                        std::unordered_map<std::string, int>& next_index,
                                        bool force_numbering);

/**
 * @brief Builds a unique move destination filename with parenthetical numeric suffixes.
 * @param desired_name Desired destination filename.
 * @param target_dir Directory where the filename must be unique.
 * @param used_names Case-insensitive names already allocated by the current move pass.
 * @param next_index Next suffix index per base filename key.
 * @return A filename that avoids current review and on-disk collisions.
 */
std::string build_unique_move_name(const std::string& desired_name,
                                   const std::filesystem::path& target_dir,
                                   std::unordered_set<std::string>& used_names,
                                   std::unordered_map<std::string, int>& next_index);

/**
 * @brief Makes rename suggestions unique across the reviewed regular-file set.
 * @param files Review entries to update in place.
 * @param base_dir Base folder used for category moves.
 * @param use_subcategory True to deduplicate per category/subcategory target.
 * @param move_categorized_entries True when categorized entries move into category folders.
 */
void ensure_unique_suggested_names(std::vector<CategorizedFile>& files,
                                   const std::string& base_dir,
                                   bool use_subcategory,
                                   bool move_categorized_entries = true);

} // namespace ReviewFileNaming
