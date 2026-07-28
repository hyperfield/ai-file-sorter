#pragma once

#include "AnalysisProgress.hpp"
#include "CategorizationProgressDialog.hpp"
#include "Types.hpp"

#include <QPointer>

#include <functional>
#include <string>
#include <vector>

/**
 * @brief Routes MainApp analysis progress updates to the progress dialog.
 *
 * The controller keeps the dialog filtering and stage-forwarding policy out of
 * MainApp while preserving MainApp's Qt dispatch context for UI-thread work.
 */
class MainAppProgressController {
public:
    using StageId = AnalysisProgressStageId;
    using StagePlan = AnalysisProgressStagePlan;
    using UiDispatcher = std::function<void(std::function<void()>)>;

    /**
     * @brief Construct a progress controller with UI dispatch callbacks.
     * @param async_dispatcher Queues non-blocking UI work.
     * @param blocking_dispatcher Runs UI work synchronously with the caller.
     */
    MainAppProgressController(UiDispatcher async_dispatcher,
                              UiDispatcher blocking_dispatcher);

    /**
     * @brief Attach the progress dialog receiving progress updates.
     * @param dialog Dialog to update, or nullptr to detach.
     */
    void set_dialog(CategorizationProgressDialog* dialog);

    /**
     * @brief Controls whether verbose vision diagnostics are visible.
     * @param show True to show development/test diagnostics.
     */
    void set_show_vision_diagnostics(bool show);

    /**
     * @brief Returns whether a progress message should be shown in the dialog.
     * @param message Progress text to evaluate.
     * @return True when the message should be visible in the dialog.
     */
    bool should_show_message_in_dialog(const std::string& message) const;

    /**
     * @brief Append a filtered progress message to the dialog.
     * @param message Progress text to append.
     */
    void append_message(const std::string& message) const;

    /**
     * @brief Configure the visible progress stages.
     * @param stages Ordered stage plan for the active analysis run.
     */
    void configure_stages(const std::vector<StagePlan>& stages) const;

    /**
     * @brief Set the files associated with a progress stage.
     * @param stage_id Stage being populated.
     * @param items Files represented by the stage.
     */
    void set_stage_items(StageId stage_id, const std::vector<FileEntry>& items) const;

    /**
     * @brief Marks the active progress stage.
     * @param stage_id Stage that is currently running.
     */
    void set_active_stage(StageId stage_id) const;

    /**
     * @brief Mark a stage item as in progress.
     * @param stage_id Stage containing the item.
     * @param entry Item to update.
     */
    void mark_stage_item_in_progress(StageId stage_id, const FileEntry& entry) const;

    /**
     * @brief Mark a stage item as completed.
     * @param stage_id Stage containing the item.
     * @param entry Item to update.
     */
    void mark_stage_item_completed(StageId stage_id, const FileEntry& entry) const;

    /**
     * @brief Mark a stage item as skipped.
     * @param stage_id Stage containing the item.
     * @param entry Item to update.
     */
    void mark_stage_item_skipped(StageId stage_id, const FileEntry& entry) const;

private:
    static bool is_vision_diagnostic(const std::string& message);
    void dispatch_async(std::function<void(CategorizationProgressDialog&)> action) const;
    void dispatch_blocking(std::function<void(CategorizationProgressDialog&)> action) const;

    QPointer<CategorizationProgressDialog> dialog_;
    UiDispatcher async_dispatcher_;
    UiDispatcher blocking_dispatcher_;
    bool show_vision_diagnostics_{false};
};
