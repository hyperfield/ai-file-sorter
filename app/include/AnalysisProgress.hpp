#pragma once

#include "Types.hpp"

#include <vector>

/**
 * @brief Logical stage identifiers used by analysis progress reporting.
 */
enum class AnalysisProgressStageId {
    ImageAnalysis,
    DocumentAnalysis,
    Categorization
};

/**
 * @brief Planned items for a single analysis progress stage.
 */
struct AnalysisProgressStagePlan {
    AnalysisProgressStageId id;
    std::vector<FileEntry> items;
};
