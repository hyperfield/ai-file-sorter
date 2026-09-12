#pragma once

#include "FolderTreeCatalog.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct FolderStructurePluginProfile;

namespace FolderStructurePattern {

/**
 * @brief Summary of naming conventions inferred from an existing folder tree.
 */
struct Profile {
    /** @brief True when at least one repeatable convention was recognized. */
    bool has_recognized_conventions{false};
    /** @brief True when top-level ranges and child numeric IDs resemble Johnny.Decimal. */
    bool has_johnny_decimal_like_ranges{false};
    /** @brief True when sibling folders commonly start with numeric prefixes. */
    bool has_numbered_prefixes{false};
    /** @brief True when sibling folders commonly start with alphabetic code prefixes. */
    bool has_alphabetic_prefixes{false};
    /** @brief Compact prompt guidance describing the inferred conventions. */
    std::string prompt_guidance;
    /** @brief Matching installed folder-structure profile ids. */
    std::vector<std::string> matched_plugin_profile_ids;
};

/**
 * @brief Convention-aware folder suggestion derived from semantic labels.
 */
struct SuggestedPath {
    /** @brief Relative target folder path using forward slashes. */
    std::string relative_path;
    /** @brief True when deterministic evidence is strong enough to use without LLM routing. */
    bool high_confidence{false};
    /** @brief Short implementation-facing reason for diagnostics and tests. */
    std::string reason;
};

/**
 * @brief Infer folder naming conventions from a catalog.
 * @param catalog Existing destination folder catalog.
 * @return Recognized conventions and optional LLM prompt guidance.
 */
Profile infer_profile(
    const FolderTreeCatalog::Catalog& catalog,
    std::span<const FolderStructurePluginProfile> plugin_profiles = {});

/**
 * @brief Suggest a new relative folder path while preserving recognized tree conventions.
 * @param catalog Existing destination folder catalog.
 * @param semantic_category Semantic main category inferred for the item.
 * @param semantic_subcategory Semantic subcategory inferred for the item.
 * @return Convention-aware path when a safe suggestion can be derived.
 */
std::optional<SuggestedPath> suggest_new_folder(
    const FolderTreeCatalog::Catalog& catalog,
    std::string_view semantic_category,
    std::string_view semantic_subcategory,
    std::span<const FolderStructurePluginProfile> plugin_profiles = {});

/**
 * @brief Return folder path text with leading numeric/range/code prefixes removed from each segment.
 * @param relative_path Relative folder path to normalize for semantic matching.
 * @return Path made from human-readable segment labels.
 */
std::string human_label_path(std::string_view relative_path);

} // namespace FolderStructurePattern
