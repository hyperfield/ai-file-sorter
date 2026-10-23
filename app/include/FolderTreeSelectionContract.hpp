#pragma once

#include "FolderTreeCatalog.hpp"
#include "LLMResponseConstraint.hpp"

/** @brief Strict JSON contracts for existing-folder choices and separately authorized proposals. */
namespace FolderTreeSelectionContract {
/** @brief Identifies the separate proposal request in prompts and audit records. */
inline constexpr std::string_view kProposalMarker = "[[AI_FILE_SORTER_NEW_FOLDER_PROPOSAL]]";
/** @brief Identifies a bounded semantic reconsideration after a contradictory existing-path proposal. */
inline constexpr std::string_view kReconsiderationMarker = "[[AI_FILE_SORTER_EXISTING_FOLDER_RECONSIDERATION]]";
/** @brief Distinguish an explicit abstention from malformed or structurally rejected output. */
enum class Outcome { Invalid, NoMatch, Selected };
/** @brief An existing-folder decision, without any new-directory side effect. */
struct Decision {
    Outcome outcome{Outcome::Invalid};
    std::optional<FolderTreeCatalog::Selection> selection;
};
/** @brief Build exact-path JSON choices plus a null abstention. @param entries Offered catalog subset. */
LLMResponseConstraint existing_constraint(std::span<const FolderTreeCatalog::Entry> entries);
/** @brief Build the JSON contract for a separately permitted new-folder proposal. */
LLMResponseConstraint proposal_constraint();
/**
 * @brief Parse only literal offered choices or an explicit null abstention.
 * @param response Raw JSON response.
 * @param candidates Exact offered paths, including pending proposals.
 * @return Selected, NoMatch, or Invalid; empty strings never mean the sorting root.
 */
Decision parse_existing(const std::string& response, const FolderTreeCatalog::Catalog& candidates);
/**
 * @brief Validate a separately requested proposal, reusing a literal existing target safely.
 * @param response Raw JSON with createFolder:true and a nonempty path.
 * @param catalog Current planning catalog.
 * @return Safe new or reused selection; convention validation and numbering remain the caller's responsibility.
 */
std::optional<FolderTreeCatalog::Selection> parse_proposal(
    const std::string& response, const FolderTreeCatalog::Catalog& catalog);
/** @brief Return stage-specific instructions for exact existing choices and abstention. */
std::string_view existing_instruction();
/** @brief Return stage-specific instructions authorizing only a new-folder proposal. */
std::string_view proposal_instruction();
/** @brief Separate evidence about artifact kind, function/audience and subject without inferring unknown facts. */
std::string_view purpose_comparison_instruction();
}
