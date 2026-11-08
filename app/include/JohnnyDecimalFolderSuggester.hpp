#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "FolderStructurePluginProfile.hpp"

namespace JohnnyDecimalFolderSuggester {

/**
 * @brief Preview for an incrementally created Johnny.Decimal folder.
 */
struct Suggestion {
    /** @brief Relative folder path using forward slashes. */
    std::string relative_path;
    /** @brief Absolute folder path on disk. */
    std::filesystem::path absolute_path;
    /** @brief Short diagnostic reason from the deterministic pattern matcher. */
    std::string reason;
};

/**
 * @brief Result of creating the suggested folder path on disk.
 */
struct CreationResult {
    /** @brief True when every needed directory was created or already available as a parent. */
    bool success{false};
    /** @brief Suggestion that was applied when successful. */
    Suggestion suggestion;
    /** @brief Directories created during the operation, deepest path last. */
    std::vector<std::filesystem::path> created_directories;
    /** @brief User-facing failure message when success is false. */
    std::string error;
};

/**
 * @brief Preview the next valid Johnny.Decimal folder for an existing archive.
 * @param root Existing archive root to scan.
 * @param area_label Human-readable top-level area label, for example Work.
 * @param folder_label Human-readable child folder label, for example Proposals.
 * @param plugin_profiles Installed folder-structure plugin profiles that may add guidance.
 * @param error Optional user-facing failure message.
 * @return Suggested folder path when the request can be safely previewed.
 */
std::optional<Suggestion> preview_next_folder(const std::filesystem::path& root, std::string_view area_label,
                                              std::string_view folder_label,
                                              std::span<const FolderStructurePluginProfile> plugin_profiles = {},
                                              std::string* error = nullptr);

/**
 * @brief Create the next valid Johnny.Decimal folder for an existing archive.
 * @param root Existing archive root to scan.
 * @param area_label Human-readable top-level area label, for example Work.
 * @param folder_label Human-readable child folder label, for example Proposals.
 * @param plugin_profiles Installed folder-structure plugin profiles that may add guidance.
 * @return Creation result with created directories or a user-facing error.
 */
CreationResult create_next_folder(const std::filesystem::path& root, std::string_view area_label,
                                  std::string_view folder_label,
                                  std::span<const FolderStructurePluginProfile> plugin_profiles = {});

}  // namespace JohnnyDecimalFolderSuggester
