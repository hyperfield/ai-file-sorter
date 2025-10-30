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
    void show_close_button();
    void update_status_column(int row, bool success, bool attempted = true);
    void on_select_all_toggled(bool checked);
    void apply_select_all(bool checked);
    void on_item_changed(QStandardItem* item);
    void update_select_all_state();
    void update_type_icon(QStandardItem* item);
    void retranslate_ui();
    void apply_status_text(QStandardItem* item) const;
    RowStatus status_from_item(const QStandardItem* item) const;
    std::vector<std::tuple<bool, std::string, std::string, std::string, std::string>> get_rows() const;

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

    bool updating_select_all{false};
};

#endif // CATEGORIZATIONDIALOG_HPP
