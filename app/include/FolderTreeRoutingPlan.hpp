#pragma once

#include "FolderTreeCatalog.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/** @brief Reserves proposed folders across every batch in one deferred review plan. */
class FolderTreeRoutingPlan {
public:
    /**
     * @brief Include reserved paths and ancestors for convention-aware number allocation.
     * @param root Destination root that scopes reservations.
     * @param existing Physically existing folders; never modified by this operation.
     * @return Planning-only catalog containing existing and proposed paths.
     */
    FolderTreeCatalog::Catalog planning_catalog(const std::string& root,
                                                const FolderTreeCatalog::Catalog& existing) const;

    /**
     * @brief Reserve a safe relative destination and all intermediate folders.
     * @param root Destination root that scopes the reservation.
     * @param relative_path Proposed path, not a filename.
     * @return False when the path is unsafe.
     */
    bool reserve(const std::string& root, const std::string& relative_path);

    /**
     * @brief Reuse a proposal with the same parent and normalized leaf label.
     * @param root Destination root that scopes reservations.
     * @param relative_path Validated candidate, before its number is reserved.
     * @param compare_human_labels Ignore detected convention prefixes when true;
     *        otherwise leading numbers/codes remain part of the literal label.
     * @return First equivalent reserved path, or empty for a different parent/label.
     */
    std::optional<std::string> find_equivalent_proposal(
        const std::string& root, const std::string& relative_path, bool compare_human_labels) const;

    /**
     * @brief Find a pending child before allocating its convention prefix.
     * @param root Destination root that scopes reservations.
     * @param parent_relative_path Resolved literal parent, or empty for a top-level folder.
     * @param label Unprefixed human label; meaningful leading numbers are preserved.
     * @param compare_human_labels Ignore convention prefixes on reserved paths when true.
     * @return Equivalent reserved child, without allocating another number.
     */
    std::optional<std::string> find_proposed_child(
        const std::string& root, const std::string& parent_relative_path,
        const std::string& label, bool compare_human_labels) const;

private:
    std::unordered_map<std::string, std::vector<FolderTreeCatalog::Entry>> reserved_;
};
