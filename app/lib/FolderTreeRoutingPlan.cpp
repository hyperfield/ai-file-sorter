#include "FolderTreeRoutingPlan.hpp"

#include <algorithm>

FolderTreeCatalog::Catalog FolderTreeRoutingPlan::planning_catalog(
    const std::string& root, const FolderTreeCatalog::Catalog& existing) const
{
    auto entries = existing.entries();
    if (const auto found = reserved_.find(root); found != reserved_.end()) {
        for (const auto& entry : found->second) {
            if (!existing.find_existing(entry.relative_path)) {
                entries.push_back(entry);
            }
        }
    }
    return FolderTreeCatalog::Catalog(std::move(entries));
}

bool FolderTreeRoutingPlan::reserve(const std::string& root, const std::string& relative_path)
{
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(relative_path);
    if (!validation.valid) {
        return false;
    }
    auto& entries = reserved_[root];
    std::size_t end = 0;
    int depth = 1;
    do {
        end = validation.normalized_path.find('/', end);
        const auto path = validation.normalized_path.substr(0, end);
        if (std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.relative_path == path;
            })) {
            entries.push_back({path, depth});
        }
        ++depth;
        if (end != std::string::npos) {
            ++end;
        }
    } while (end != std::string::npos);
    return true;
}
