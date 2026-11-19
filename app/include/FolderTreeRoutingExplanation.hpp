#pragma once

#include "FolderStructurePattern.hpp"
#include "FolderTreeCatalog.hpp"

#include <optional>
#include <string>

namespace FolderTreeRoutingExplanation {

/**
 * @brief Inputs used to explain an existing-folder-tree routing decision.
 */
struct Context {
    /** @brief Semantic category used for routing. */
    std::string semantic_category;
    /** @brief Semantic subcategory used for routing. */
    std::string semantic_subcategory;
    /** @brief Default semantic target path derived from the category pair. */
    std::string semantic_target_folder;
    /** @brief Final target folder selected for the item. */
    FolderTreeCatalog::Selection selection;
    /** @brief Closest existing folder match, when one was available. */
    std::optional<FolderTreeCatalog::SemanticMatch> best_existing;
    /** @brief Folder naming conventions inferred from the destination tree. */
    FolderStructurePattern::Profile structure_profile;
};

/**
 * @brief Build a user-facing explanation for a folder-tree routing decision.
 * @param context Routing inputs and selected destination.
 * @return Multi-line explanation suitable for progress text or a tooltip.
 */
std::string build(const Context& context);

} // namespace FolderTreeRoutingExplanation
