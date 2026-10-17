#pragma once

#include "FolderTreeCatalog.hpp"
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

private:
    std::unordered_map<std::string, std::vector<FolderTreeCatalog::Entry>> reserved_;
};
