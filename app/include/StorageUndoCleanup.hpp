#pragma once

#include <cstddef>
#include <string>
#include <vector>

class IStorageProvider;

namespace StorageUndoCleanup {

/**
 * @brief Returns target-directory ancestors that do not exist before a move.
 * @param provider Storage provider used to test path existence.
 * @param target_directory Destination directory that may need to be created.
 * @return Missing directories in parent-to-child order.
 */
std::vector<std::string> missing_directories_for_target(const IStorageProvider& provider,
                                                        const std::string& target_directory);

/**
 * @brief Returns recorded created directories that contain a target directory.
 * @param created_directories Directories already known to have been app-created.
 * @param target_directory Destination directory for the current move.
 * @return Relevant created directories in parent-to-child order.
 */
std::vector<std::string> recorded_directories_for_target(
    const std::vector<std::string>& created_directories,
    const std::string& target_directory);

/**
 * @brief Removes only app-created directories that are currently empty.
 * @param directories Directories recorded as created by the app.
 * @return Number of directories removed.
 */
std::size_t remove_empty_created_directories(const std::vector<std::string>& directories);

/**
 * @brief Removes empty parent directories above a moved-from path.
 * @param moved_from Path that used to contain the moved entry.
 * @return Number of empty parent directories removed.
 */
std::size_t remove_empty_parent_directories(const std::string& moved_from);

} // namespace StorageUndoCleanup
