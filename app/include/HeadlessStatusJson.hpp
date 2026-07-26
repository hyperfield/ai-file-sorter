#ifndef HEADLESS_STATUS_JSON_HPP
#define HEADLESS_STATUS_JSON_HPP

#include "AnalysisRuntimeLock.hpp"
#include "HeadlessAnalysisCommand.hpp"
#include "HeadlessReviewApplyService.hpp"
#include "Types.hpp"

#include <QJsonObject>
#include <QString>

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace HeadlessStatusJson {

/**
 * @brief Review plan loaded from or written to the headless review JSON contract.
 */
struct ReviewPlan {
    /** @brief Operation that produced the review entries. */
    HeadlessAnalysisCommand::Operation operation{HeadlessAnalysisCommand::Operation::Unknown};
    /** @brief Original headless target paths recorded in the review plan. */
    std::vector<std::filesystem::path> paths;
    /** @brief Apply options to replay when the plan is approved. */
    HeadlessReviewApplyService::Options apply_options;
    /** @brief Categorized entries to apply from the saved review. */
    std::vector<CategorizedFile> entries;
};

/**
 * @brief Convert a filesystem path to the UTF-8 QString form used in headless JSON.
 * @param path Path to serialize.
 * @return QString containing the UTF-8 path text.
 */
QString path_to_json_string(const std::filesystem::path& path);

/**
 * @brief Convert an apply result into the common headless status payload.
 * @param result Review/apply result to serialize.
 * @return JSON payload containing review and apply sections.
 */
QJsonObject apply_result_to_json(const HeadlessReviewApplyService::Result& result);

/**
 * @brief Convert an apply result into a status payload with review-required metadata.
 * @param result Review/apply result to serialize.
 * @param review_required Whether approval is still required before applying changes.
 * @return JSON payload containing review, apply, and approval state.
 */
QJsonObject apply_result_to_json(const HeadlessReviewApplyService::Result& result,
                                 bool review_required);

/**
 * @brief Emit a headless status object to stdout and, when configured, to disk.
 * @param options Headless command options that identify operation, paths, job, and status file.
 * @param status Stable status value such as running, completed, failed, or review_required.
 * @param message Human-readable status message.
 * @param error Optional human-readable error detail.
 * @param lock_metadata Runtime lock metadata to include, when available.
 * @param out Machine-readable output stream.
 * @param err Diagnostic output stream.
 * @param extra_payload Additional JSON fields to merge into the status object.
 * @return True when stdout was written and the optional status file was written successfully.
 */
bool emit_status(const HeadlessAnalysisCommand::Options& options,
                 const std::string& status,
                 const std::string& message,
                 const std::string& error,
                 const std::optional<AnalysisRuntimeLock::Metadata>& lock_metadata,
                 std::ostream& out,
                 std::ostream& err,
                 const QJsonObject* extra_payload = nullptr);

/**
 * @brief Resolve the review-plan path for a headless job.
 * @param options Headless command options.
 * @param runtime_dir Runtime directory used when no status/review file was supplied.
 * @return Review-plan path, or nullopt if no deterministic path can be resolved.
 */
std::optional<std::filesystem::path>
review_file_path_for(const HeadlessAnalysisCommand::Options& options,
                     const std::filesystem::path& runtime_dir);

/**
 * @brief Write a headless review plan JSON file.
 * @param path Destination review-plan path.
 * @param options Source command options.
 * @param entries Review entries to persist.
 * @param apply_options Apply options to replay later.
 * @param error Receives a diagnostic on failure.
 * @return True when the file was written successfully.
 */
bool write_review_plan_file(const std::filesystem::path& path,
                            const HeadlessAnalysisCommand::Options& options,
                            const std::vector<CategorizedFile>& entries,
                            const HeadlessReviewApplyService::Options& apply_options,
                            std::string* error);

/**
 * @brief Read and validate a headless review plan JSON file.
 * @param path Source review-plan path.
 * @param plan Receives the parsed review plan.
 * @param error Receives a diagnostic on failure.
 * @return True when the plan was parsed and contains at least one entry.
 */
bool read_review_plan_file(const std::filesystem::path& path,
                           ReviewPlan* plan,
                           std::string* error);

} // namespace HeadlessStatusJson

#endif // HEADLESS_STATUS_JSON_HPP
