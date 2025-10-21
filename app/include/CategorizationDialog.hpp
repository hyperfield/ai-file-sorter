#ifndef CATEGORIZATIONDIALOG_HPP
#define CATEGORIZATIONDIALOG_HPP

#include "Types.hpp"

#include <QDialog>

#include <memory>
#include <tuple>
#include <vector>
#include <spdlog/logger.h>

class DatabaseManager;
class QCloseEvent;
class QPushButton;
class QStandardItemModel;
class QTableView;

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

private:
    void setup_ui();
    void populate_model();
    void record_categorization_to_db();
    void on_confirm_and_sort_button_clicked();
    void on_continue_later_button_clicked();
    void show_close_button();
    void update_status_column(int row, bool success);
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> get_rows() const;

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
};

#endif // CATEGORIZATIONDIALOG_HPP
