#pragma once

#include "FolderTreeCatalog.hpp"
#include "LLMResponseConstraint.hpp"
#include <functional>

/** @brief Per-request experimental controls; defaults retain descriptions without stricter comparison passes. */
struct FolderTreeRoutingOptions {
    /** @brief Compare artifact kind, function/audience and subject against folder intent. */
    bool compare_purpose{false};
    /** @brief Reconsider existing paths proposed after abstention instead of trusting their validity alone. */
    bool verify_existing_proposals{false};
};

/** @brief Content-driven destination comparisons; the model, not lexical scores, chooses every finalist. */
class FolderTreeLLMRouter {
public:
    using Request = std::function<std::string(const std::string&, const LLMResponseConstraint&)>;
    /** @brief Validate/allocate a model-selected target; empty triggers the bounded corrective retry. */
    using ValidateSelection = std::function<std::optional<FolderTreeCatalog::Selection>(
        FolderTreeCatalog::Selection)>;

    /**
     * @brief Compare every catalog entry in bounded groups and then compare their model-selected winners.
     * @param catalog Existing folders plus pending review proposals.
     * @param allow_new Permit a separate proposal only after explicit existing-folder abstention.
     * @param category Preliminary category hint, never an enforced destination.
     * @param subcategory Preliminary subcategory hint.
     * @param profiles Declarative naming guidance.
     * @param request LLM callback that includes the same original content evidence on every call.
     * @param validate Optional structural validation, performed before accepting each response.
     * @param options Injectable comparison policy; nondefault controls support isolated protocol measurements.
     * @return Valid model selection; empty after bounded invalid-response retries.
     * @throws PromptBudgetError When even a minimal comparison cannot fit intact.
     */
    static std::optional<FolderTreeCatalog::Selection> select(
        const FolderTreeCatalog::Catalog& catalog, bool allow_new,
        const std::string& category, const std::string& subcategory,
        std::span<const FolderStructurePluginProfile> profiles, const Request& request,
        const ValidateSelection& validate = {}, FolderTreeRoutingOptions options = {});
};
