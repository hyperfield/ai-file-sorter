#include "FolderTreeLLMRouter.hpp"
#include "LLMErrors.hpp"

#include <array>

namespace {
// Bound remote comparisons, but let measured token budgets split smaller local catalogs only when needed.
constexpr std::size_t kComparisonFolderLimit = 128;
constexpr int kResponseAttempts = 2;

struct Comparison {
    const FolderTreeCatalog::Catalog& catalog;
    const std::string& category;
    const std::string& subcategory;
    std::span<const FolderStructurePluginProfile> profiles;
    const FolderTreeLLMRouter::Request& request;

    std::optional<FolderTreeCatalog::Selection> ask(
        std::span<const FolderTreeCatalog::Entry> entries, bool allow_new) const
    {
        auto context = FolderTreeCatalog::build_candidate_prompt_context(
            catalog, entries, allow_new, category, subcategory, profiles);
        const FolderTreeCatalog::Catalog candidates({entries.begin(), entries.end()});
        for (int attempt = 0; attempt < kResponseAttempts; ++attempt) {
            auto selected = FolderTreeCatalog::parse_selection(request(context), catalog, allow_new);
            if (selected && (selected->suggested_new || candidates.find_existing(selected->relative_path))) {
                return selected;
            }
            context += "\nInvalid response. Return one JSON object, use a listed literal existing path, and obey the new-folder permission.\n";
        }
        return std::nullopt;
    }

    std::optional<FolderTreeCatalog::Selection> compare(
        std::span<const FolderTreeCatalog::Entry> entries, bool allow_new) const
    {
        if (entries.empty() && !allow_new) return std::nullopt;
        if (entries.size() <= kComparisonFolderLimit) {
            try {
                return ask(entries, allow_new);
            } catch (const PromptBudgetError&) {
                // Two finalists must fit together; do not recurse forever on mandatory text.
                if (entries.size() <= 2) throw;
            }
        }
        const auto middle = entries.size() / 2;
        const auto left = compare(entries.first(middle), false);
        const auto right = compare(entries.subspan(middle), false);
        if (!left || !right) return std::nullopt;
        const std::array<FolderTreeCatalog::Entry, 2> finalists = {{{left->relative_path}, {right->relative_path}}};
        return ask(finalists, allow_new);
    }
};
} // namespace

std::optional<FolderTreeCatalog::Selection> FolderTreeLLMRouter::select(
    const FolderTreeCatalog::Catalog& catalog, bool allow_new,
    const std::string& category, const std::string& subcategory,
    std::span<const FolderStructurePluginProfile> profiles, const Request& request)
{
    return Comparison{catalog, category, subcategory, profiles, request}.compare(catalog.entries(), allow_new);
}
