#ifndef CATEGORIZATIONDIALOG_HPP
#define CATEGORIZATIONDIALOG_HPP

#include "Types.hpp"

#include <QDialog>
#include <QStandardItemModel>

#include <memory>
#include <tuple>
#include <vector>
#include <spdlog/logger.h>

class DatabaseManager;
class QCloseEvent;
class QEvent;
class QPushButton;
class QTableView;
class QCheckBox;
class QStandardItem;

class CategorizationDialog : public QDialog
{
public:
    CategorizationDialog(DatabaseManager* db_manager,
                         bool show_subcategory_col,
                         QWidget* parent = nullptr);

    void set_show_subcategory_column(bool enabled);
    bool show_subcategory_column_enabled() const { return show_subcategory_column; }

#ifdef AI_FILE_SORTER_TEST_BUILD
    void test_set_entries(const std::vector<CategorizedFile>& files);
    void test_trigger_confirm();
    void test_trigger_undo();
    bool test_undo_enabled() const;
#endif

    bool is_dialog_valid() const;
    void show_results(const std::vector<CategorizedFile>& categorized_files);

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum class RowStatus {
        None = 0,
        Moved,
        Skipped,
        NotSelected
    };

    static constexpr int kStatusRole = Qt::UserRole + 100;

    void setup_ui();
    void populate_model();
    void record_categorization_to_db();
    void on_confirm_and_sort_button_clicked();
    void on_continue_later_button_clicked();
    void on_undo_button_clicked();
    void show_close_button();
    void restore_action_buttons();
    void update_status_column(int row, bool success, bool attempted = true);
    void on_select_all_toggled(bool checked);
    void apply_select_all(bool checked);
    void on_item_changed(QStandardItem* item);
    void update_select_all_state();
    void update_type_icon(QStandardItem* item);
    void retranslate_ui();
    void apply_status_text(QStandardItem* item) const;
    RowStatus status_from_item(const QStandardItem* item) const;
    std::vector<std::tuple<bool, std::string, std::string, std::string>> get_rows() const;
    void on_show_subcategories_toggled(bool checked);
    void apply_subcategory_visibility();
    void clear_move_history();
    void record_move_for_undo(int row, const std::string& source, const std::string& destination);
    void handle_selected_row(int row_index,
                             const std::string& file_name,
                             const std::string& category,
                             const std::string& subcategory,
                             const std::string& base_dir,
                             std::vector<std::string>& files_not_moved);
    bool undo_move_history();
    void update_status_after_undo();
    bool move_file_back(const std::string& source, const std::string& destination);
    void remove_empty_parent_directories(const std::string& destination);

    DatabaseManager* db_manager;
    bool show_subcategory_column;
    std::vector<CategorizedFile> categorized_files;

    std::shared_ptr<spdlog::logger> core_logger;
    std::shared_ptr<spdlog::logger> db_logger;
    std::shared_ptr<spdlog::logger> ui_logger;

    QTableView* table_view{nullptr};
    QStandardItemModel* model{nullptr};
    QPushButton* confirm_button{nullptr};
    QPushButton* continue_button{nullptr};
    QPushButton* close_button{nullptr};
    QCheckBox* select_all_checkbox{nullptr};
    QCheckBox* show_subcategories_checkbox{nullptr};
    QPushButton* undo_button{nullptr};

    struct MoveRecord {
        int row_index;
        std::string source_path;
        std::string destination_path;
    };
    std::vector<MoveRecord> move_history_;

    bool updating_select_all{false};
};

#endif // CATEGORIZATIONDIALOG_HPP
