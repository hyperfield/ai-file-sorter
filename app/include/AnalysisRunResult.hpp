#pragma once

#include <string>

/**
 * @brief Terminal state for an analysis workflow run.
 */
enum class AnalysisRunStatus {
    Completed,
    Cancelled,
    Failed
};

/**
 * @brief UI-neutral result returned by an analysis workflow run.
 */
struct AnalysisRunResult {
    AnalysisRunStatus status{AnalysisRunStatus::Completed};
    std::string error_message;
};
