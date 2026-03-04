#ifndef CATEGORIZATIONPROGRESSDIALOG_HPP
#define CATEGORIZATIONPROGRESSDIALOG_HPP

#include "Types.hpp"

#include <QDialog>

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class MainApp;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QEvent;

class CategorizationProgressDialog : public QDialog
{
public:
    enum class StageId {
        ImageAnalysis,
        DocumentAnalysis,
        Categorization
    };

    struct StagePlan {
        StageId id;
        std::vector<FileEntry> items;
    };

    CategorizationProgressDialog(QWidget* parent, MainApp* main_app, bool show_subcategory_col);

    void show();
    void hide();
    void append_text(const std::string& text);
    void configure_stages(const std::vector<StagePlan>& stages);
    void set_stage_items(StageId stage_id, const std::vector<FileEntry>& items);
    void set_active_stage(StageId stage_id);
    void mark_stage_item_pending(StageId stage_id, const FileEntry& entry);
    void mark_stage_item_in_progress(StageId stage_id, const FileEntry& entry);
    void mark_stage_item_completed(StageId stage_id, const FileEntry& entry);

protected:
    void changeEvent(QEvent* event) override;

private:
    enum class ItemStatus {
        NotApplicable,
        Pending,
        InProgress,
        Completed
    };

    enum class DisplayType {
        Directory,
        File,
        Image,
        Document
    };

    struct StageState {
        bool enabled{false};
        std::unordered_set<std::string> item_keys;
    };

    struct ItemState {
        int row{-1};
        DisplayType display_type{DisplayType::File};
        std::array<ItemStatus, 3> stage_statuses{
            ItemStatus::NotApplicable,
            ItemStatus::NotApplicable,
            ItemStatus::NotApplicable
        };
    };

    static constexpr int kStageCount = 3;

    void setup_ui(bool show_subcategory_col);
    void retranslate_ui();
    void request_stop();
    static std::string make_item_key(const std::string& full_path, FileType type);
    static std::size_t stage_index(StageId stage_id);
    QString stage_label(StageId stage_id) const;
    static DisplayType classify_display_type(const FileEntry& entry);
    QString display_type_label(DisplayType display_type) const;
    int column_for_stage(StageId stage_id) const;
    void ensure_stage_enabled(StageId stage_id);
    void upsert_stage_item(StageId stage_id, const FileEntry& entry);
    void upsert_item(const FileEntry& entry);
    void set_stage_item_status(StageId stage_id, const FileEntry& entry, ItemStatus status);
    void rebuild_headers();
    void refresh_stage_overview();
    void refresh_row(int row);
    void refresh_summary();
    void refresh_spinner();
    bool has_in_progress_item() const;
    ItemStatus stage_status_for_row(const ItemState& state, StageId stage_id) const;
    std::optional<int> find_stage_row(StageId stage_id, ItemStatus status) const;
    void ensure_row_visible(int row);

    MainApp* main_app;
    QLabel* stage_list_label{nullptr};
    QLabel* summary_label{nullptr};
    QLabel* log_label{nullptr};
    QTableWidget* status_table{nullptr};
    QPlainTextEdit* text_view{nullptr};
    QPushButton* stop_button{nullptr};
    QTimer* spinner_timer{nullptr};
    std::array<StageState, kStageCount> stage_states_{};
    std::vector<StageId> active_stage_order_;
    std::optional<StageId> active_stage_;
    std::unordered_map<std::string, ItemState> item_states_;
    int spinner_frame_index_{0};
};

#endif // CATEGORIZATIONPROGRESSDIALOG_HPP
