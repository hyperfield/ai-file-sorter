#include "FolderTreeRoutingPlan.hpp"
#include "FolderStructurePattern.hpp"

#include <QString>
#include <algorithm>
#include <utility>

namespace {
using ProposalIdentity = std::pair<std::string, std::string>;

std::optional<ProposalIdentity> proposal_identity(const std::string& path, bool compare_human_labels)
{
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
    if (!validation.valid) {
        return std::nullopt;
    }
    const auto separator = validation.normalized_path.find_last_of('/');
    const auto parent = separator == std::string::npos ? std::string()
        : validation.normalized_path.substr(0, separator);
    const auto leaf = validation.normalized_path.substr(separator == std::string::npos ? 0 : separator + 1);
    const auto label = compare_human_labels ? FolderStructurePattern::human_label_path(leaf) : leaf;
    return ProposalIdentity{parent, QString::fromStdString(label).normalized(QString::NormalizationForm_C)
                                      .simplified().toCaseFolded().toStdString()};
}
} // namespace

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

std::optional<std::string> FolderTreeRoutingPlan::find_equivalent_proposal(
    const std::string& root, const std::string& relative_path, bool compare_human_labels) const
{
    const auto identity = proposal_identity(relative_path, compare_human_labels);
    return identity ? find_proposed_child(root, identity->first, identity->second, compare_human_labels)
                    : std::nullopt;
}

std::optional<std::string> FolderTreeRoutingPlan::find_proposed_child(
    const std::string& root, const std::string& parent_relative_path,
    const std::string& label, bool compare_human_labels) const
{
    if (label.empty() || label.find_first_of("/\\") != std::string::npos) {
        return std::nullopt;
    }
    const auto path = parent_relative_path.empty() ? label : parent_relative_path + "/" + label;
    const auto identity = proposal_identity(path, false);
    const auto found = reserved_.find(root);
    if (!identity || found == reserved_.end()) {
        return std::nullopt;
    }
    for (const auto& entry : found->second) {
        if (proposal_identity(entry.relative_path, compare_human_labels) == identity) {
            return entry.relative_path;
        }
    }
    return std::nullopt;
}
