#pragma once

#include "FolderTreeCatalog.hpp"
#include <functional>

/** @brief Content-driven destination comparisons; the model, not lexical scores, chooses every finalist. */
class FolderTreeLLMRouter {
public:
    using Request = std::function<std::string(const std::string&)>;

    /**
     * @brief Compare every catalog entry in bounded groups and then compare their model-selected winners.
     * @param catalog Existing folders plus pending review proposals.
     * @param allow_new Permit new destinations only in the final comparison.
     * @param category Preliminary category hint, never an enforced destination.
     * @param subcategory Preliminary subcategory hint.
     * @param profiles Declarative naming guidance.
     * @param request LLM callback that includes the same original content evidence on every call.
     * @return Valid model selection; empty after bounded invalid-response retries.
     * @throws PromptBudgetError When even a minimal comparison cannot fit intact.
     */
    static std::optional<FolderTreeCatalog::Selection> select(
        const FolderTreeCatalog::Catalog& catalog, bool allow_new,
        const std::string& category, const std::string& subcategory,
        std::span<const FolderStructurePluginProfile> profiles, const Request& request);
};
