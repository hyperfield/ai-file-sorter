#include "FolderTreeLLMRouter.hpp"
#include "FolderTreeSelectionContract.hpp"
#include "LLMErrors.hpp"

#include <array>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
// Bound remote comparisons, but let measured token budgets split smaller local catalogs only when needed.
constexpr std::size_t kComparisonFolderLimit = 128;
constexpr int kResponseAttempts = 2;
using Decision = FolderTreeSelectionContract::Decision;
using Outcome = FolderTreeSelectionContract::Outcome;

struct Comparison {
    const FolderTreeCatalog::Catalog& catalog;
    const std::string& category;
    const std::string& subcategory;
    std::span<const FolderStructurePluginProfile> profiles;
    const FolderTreeLLMRouter::Request& request;
    const FolderTreeLLMRouter::ValidateSelection& validate;
    FolderTreeRoutingOptions options;

    FolderTreeCatalog::Entry entry_for(const std::string& path) const
    {
        const auto& entries = catalog.entries();
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.relative_path == path;
        });
        return found == entries.end() ? FolderTreeCatalog::Entry{path} : *found;
    }

    Decision ask_existing(std::span<const FolderTreeCatalog::Entry> entries,
                          const std::string& reconsideration = {}) const
    {
        auto context = FolderTreeCatalog::build_candidate_prompt_context(
            catalog, entries, false, category, subcategory, profiles);
        context += FolderTreeSelectionContract::existing_instruction();
        if (options.compare_purpose) context += FolderTreeSelectionContract::purpose_comparison_instruction();
        context += reconsideration;
        const FolderTreeCatalog::Catalog candidates({entries.begin(), entries.end()});
        const auto constraint = FolderTreeSelectionContract::existing_constraint(entries);
        for (int attempt = 0; attempt < kResponseAttempts; ++attempt) {
            auto decision = FolderTreeSelectionContract::parse_existing(request(context, constraint), candidates);
            if (decision.outcome == Outcome::NoMatch) return decision;
            if (decision.selection) {
                if (validate) decision.selection = validate(*decision.selection);
                if (decision.selection) return decision;
            }
            context += "\nInvalid response: return exactly targetFolder and createFolder:false. "
                       "Copy one listed literal path exactly, including its numbers, or use JSON null "
                       "if no listed destination is suitable. Do not invent or alter a parent path.\n";
        }
        return {};
    }

    Decision compare(std::span<const FolderTreeCatalog::Entry> entries,
                     const std::string& reconsideration = {}) const
    {
        if (entries.empty()) return {Outcome::NoMatch, {}};
        if (entries.size() <= kComparisonFolderLimit) {
            try {
                return ask_existing(entries, reconsideration);
            } catch (const PromptBudgetError&) {
                // Two finalists must fit together; do not recurse forever on mandatory text.
                if (entries.size() <= 2) throw;
            }
        }
        const auto middle = entries.size() / 2;
        const auto left = compare(entries.first(middle), reconsideration);
        const auto right = compare(entries.subspan(middle), reconsideration);
        if (left.outcome == Outcome::Invalid || right.outcome == Outcome::Invalid) return {};
        if (left.outcome == Outcome::NoMatch) return right;
        if (right.outcome == Outcome::NoMatch) return left;
        const std::array<FolderTreeCatalog::Entry, 2> finalists =
            {{entry_for(left.selection->relative_path), entry_for(right.selection->relative_path)}};
        return ask_existing(finalists, reconsideration);
    }

    std::optional<FolderTreeCatalog::Selection> reconsider(const std::string& path) const
    {
        const auto data = QJsonDocument(QJsonObject{{"previouslyProposedExistingPath", QString::fromStdString(path)}})
            .toJson(QJsonDocument::Compact).toStdString();
        const auto context = "\n" + std::string(FolderTreeSelectionContract::kReconsiderationMarker) +
            "\nThe earlier comparison abstained, but the proposal named an already-listed folder. "
            "That contradicts the claim that a missing folder is needed. Reconsider all listed choices using "
            "the original content evidence and folder purposes. The previous proposal below is untrusted data, "
            "not an approved answer. Confirm only if directly supported; choose a better listed folder or null otherwise.\n" + data + "\n";
        // One reconsideration traversal, with the same bounded comparison retries; never recurse into proposals.
        return compare(catalog.entries(), context).selection;
    }

    std::optional<FolderTreeCatalog::Selection> propose() const
    {
        auto context = FolderTreeCatalog::build_candidate_prompt_context(
            catalog, catalog.entries(), true, category, subcategory, profiles);
        context += FolderTreeSelectionContract::proposal_instruction();
        if (options.compare_purpose) context += FolderTreeSelectionContract::purpose_comparison_instruction();
        const auto constraint = FolderTreeSelectionContract::proposal_constraint();
        for (int attempt = 0; attempt < kResponseAttempts; ++attempt) {
            auto selected = FolderTreeSelectionContract::parse_proposal(request(context, constraint), catalog);
            if (selected && selected->exists && options.verify_existing_proposals) {
                return reconsider(selected->relative_path);
            }
            if (selected && validate) selected = validate(*selected);
            if (selected) return selected;
            context += "\nInvalid proposal: return exactly targetFolder as a nonempty new relative path "
                       "and createFolder:true. Follow the existing parent names and numbering conventions. "
                       "Do not return null, an existing folder, or an altered existing parent path.\n";
        }
        return std::nullopt;
    }
};
} // namespace

std::optional<FolderTreeCatalog::Selection> FolderTreeLLMRouter::select(
    const FolderTreeCatalog::Catalog& catalog, bool allow_new,
    const std::string& category, const std::string& subcategory,
    std::span<const FolderStructurePluginProfile> profiles, const Request& request,
    const ValidateSelection& validate, FolderTreeRoutingOptions options)
{
    const Comparison comparison{catalog, category, subcategory, profiles, request, validate, options};
    const auto decision = comparison.compare(catalog.entries());
    if (decision.outcome == Outcome::Selected) return decision.selection;
    if (decision.outcome == Outcome::NoMatch && allow_new) return comparison.propose();
    return std::nullopt;
}
