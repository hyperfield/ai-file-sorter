#include "MainAppProgressController.hpp"

#include <utility>

namespace {

bool has_prefix(const std::string& value, const char* prefix)
{
    return value.rfind(prefix, 0) == 0;
}

} // namespace

MainAppProgressController::MainAppProgressController(UiDispatcher async_dispatcher,
                                                     UiDispatcher blocking_dispatcher)
    : async_dispatcher_(std::move(async_dispatcher)),
      blocking_dispatcher_(std::move(blocking_dispatcher))
{
}

void MainAppProgressController::set_dialog(CategorizationProgressDialog* dialog)
{
    dialog_ = dialog;
}

void MainAppProgressController::set_show_vision_diagnostics(bool show)
{
    show_vision_diagnostics_ = show;
}

bool MainAppProgressController::should_show_message_in_dialog(const std::string& message) const
{
    return !is_vision_diagnostic(message) || show_vision_diagnostics_;
}

void MainAppProgressController::append_message(const std::string& message) const
{
    if (!should_show_message_in_dialog(message)) {
        return;
    }

    dispatch_async([message](CategorizationProgressDialog& dialog) {
        dialog.append_text(message);
    });
}

void MainAppProgressController::configure_stages(const std::vector<StagePlan>& stages) const
{
    dispatch_blocking([stages](CategorizationProgressDialog& dialog) {
        dialog.configure_stages(stages);
    });
}

void MainAppProgressController::set_stage_items(StageId stage_id,
                                                const std::vector<FileEntry>& items) const
{
    dispatch_blocking([stage_id, items](CategorizationProgressDialog& dialog) {
        dialog.set_stage_items(stage_id, items);
    });
}

void MainAppProgressController::set_active_stage(StageId stage_id) const
{
    dispatch_blocking([stage_id](CategorizationProgressDialog& dialog) {
        dialog.set_active_stage(stage_id);
    });
}

void MainAppProgressController::mark_stage_item_in_progress(StageId stage_id,
                                                            const FileEntry& entry) const
{
    dispatch_blocking([stage_id, entry](CategorizationProgressDialog& dialog) {
        dialog.mark_stage_item_in_progress(stage_id, entry);
    });
}

void MainAppProgressController::mark_stage_item_completed(StageId stage_id,
                                                          const FileEntry& entry) const
{
    dispatch_blocking([stage_id, entry](CategorizationProgressDialog& dialog) {
        dialog.mark_stage_item_completed(stage_id, entry);
    });
}

void MainAppProgressController::mark_stage_item_skipped(StageId stage_id,
                                                        const FileEntry& entry) const
{
    dispatch_blocking([stage_id, entry](CategorizationProgressDialog& dialog) {
        dialog.mark_stage_item_skipped(stage_id, entry);
    });
}

bool MainAppProgressController::is_vision_diagnostic(const std::string& message)
{
    return has_prefix(message, "[VISION] Runtime: ") ||
           has_prefix(message, "[VISION] Timing ");
}

void MainAppProgressController::dispatch_async(
    std::function<void(CategorizationProgressDialog&)> action) const
{
    if (!async_dispatcher_) {
        return;
    }

    async_dispatcher_([this, action = std::move(action)]() mutable {
        if (dialog_) {
            action(*dialog_);
        }
    });
}

void MainAppProgressController::dispatch_blocking(
    std::function<void(CategorizationProgressDialog&)> action) const
{
    if (!blocking_dispatcher_) {
        return;
    }

    blocking_dispatcher_([this, action = std::move(action)]() mutable {
        if (dialog_) {
            action(*dialog_);
        }
    });
}
