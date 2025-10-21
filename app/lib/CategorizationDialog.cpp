#include "CategorizationDialog.hpp"

#include "DatabaseManager.hpp"
#include "Logger.hpp"
#include "MovableCategorizedFile.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QCloseEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <fmt/format.h>

#include <filesystem>

CategorizationDialog::CategorizationDialog(DatabaseManager* db_manager,
                                           bool show_subcategory_col,
                                           QWidget* parent)
    : QDialog(parent),
      db_manager(db_manager),
      show_subcategory_column(show_subcategory_col),
      core_logger(Logger::get_logger("core_logger")),
      db_logger(Logger::get_logger("db_logger")),
      ui_logger(Logger::get_logger("ui_logger"))
{
    setWindowTitle(tr("Review Categorization"));
    resize(1100, 720);
    setup_ui();
}


bool CategorizationDialog::is_dialog_valid() const
{
    return model != nullptr && table_view != nullptr;
}


void CategorizationDialog::show_results(const std::vector<CategorizedFile>& files)
{
    categorized_files = files;
    populate_model();
    exec();
}


void CategorizationDialog::setup_ui()
{
    auto* layout = new QVBoxLayout(this);

    model = new QStandardItemModel(this);
    model->setColumnCount(5);
    model->setHorizontalHeaderLabels({
        tr("File"),
        tr("Type"),
        tr("Category"),
        tr("Subcategory"),
        tr("Status")
    });

    table_view = new QTableView(this);
    table_view->setModel(model);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    table_view->horizontalHeader()->setStretchLastSection(true);
    table_view->verticalHeader()->setVisible(false);
    table_view->setColumnHidden(1, false);
    table_view->setColumnHidden(3, !show_subcategory_column);
    layout->addWidget(table_view, 1);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);

    confirm_button = new QPushButton(tr("Confirm and Sort"), this);
    continue_button = new QPushButton(tr("Continue Later"), this);
    close_button = new QPushButton(tr("Close"), this);
    close_button->setVisible(false);

    button_layout->addWidget(confirm_button);
    button_layout->addWidget(continue_button);
    button_layout->addWidget(close_button);

    layout->addLayout(button_layout);

    connect(confirm_button, &QPushButton::clicked, this, &CategorizationDialog::on_confirm_and_sort_button_clicked);
    connect(continue_button, &QPushButton::clicked, this, &CategorizationDialog::on_continue_later_button_clicked);
    connect(close_button, &QPushButton::clicked, this, &CategorizationDialog::accept);
}


void CategorizationDialog::populate_model()
{
    model->removeRows(0, model->rowCount());

    for (const auto& file : categorized_files) {
        QList<QStandardItem*> row;

        auto* file_item = new QStandardItem(QString::fromStdString(file.file_name));
        file_item->setEditable(false);
        file_item->setData(QString::fromStdString(file.file_path), Qt::UserRole + 1);

        auto* type_item = new QStandardItem(file.type == FileType::Directory ? tr("Directory") : tr("File"));
        type_item->setEditable(false);
        type_item->setData(file.type == FileType::Directory ? QStringLiteral("D") : QStringLiteral("F"), Qt::UserRole);

        auto* category_item = new QStandardItem(QString::fromStdString(file.category));
        category_item->setEditable(true);

        auto* subcategory_item = new QStandardItem(QString::fromStdString(file.subcategory));
        subcategory_item->setEditable(true);

        auto* status_item = new QStandardItem;
        status_item->setEditable(false);

        row << file_item << type_item << category_item << subcategory_item << status_item;
        model->appendRow(row);
    }

    table_view->setColumnHidden(3, !show_subcategory_column);
    table_view->resizeColumnsToContents();
}


void CategorizationDialog::record_categorization_to_db()
{
    if (!db_manager) {
        return;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        if (row >= static_cast<int>(categorized_files.size())) {
            break;
        }

        auto& entry = categorized_files[static_cast<size_t>(row)];
        std::string category = model->item(row, 2)->text().toStdString();
        std::string subcategory = show_subcategory_column
                                      ? model->item(row, 3)->text().toStdString()
                                      : "";

        auto resolved = db_manager->resolve_category(category, subcategory);

        const std::string file_type = (entry.type == FileType::Directory) ? "D" : "F";
        db_manager->insert_or_update_file_with_categorization(
            entry.file_name, file_type, entry.file_path, resolved);

        entry.category = resolved.category;
        entry.subcategory = resolved.subcategory;
        entry.taxonomy_id = resolved.taxonomy_id;

        model->item(row, 2)->setText(QString::fromStdString(resolved.category));
        if (show_subcategory_column) {
            model->item(row, 3)->setText(QString::fromStdString(resolved.subcategory));
        }
    }
}


std::vector<std::tuple<std::string, std::string, std::string, std::string>>
CategorizationDialog::get_rows() const
{
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> rows;
    rows.reserve(model->rowCount());

    for (int row = 0; row < model->rowCount(); ++row) {
        const QString file_name = model->item(row, 0)->text();
        const QString file_type = model->item(row, 1)->data(Qt::UserRole).toString();
        const QString category = model->item(row, 2)->text();
        const QString subcategory = show_subcategory_column
                                        ? model->item(row, 3)->text()
                                        : QString();
        rows.emplace_back(file_name.toStdString(),
                          file_type.toStdString(),
                          category.toStdString(),
                          subcategory.toStdString());
    }

    return rows;
}


void CategorizationDialog::on_confirm_and_sort_button_clicked()
{
    record_categorization_to_db();

    if (categorized_files.empty()) {
        if (ui_logger) {
            ui_logger->warn("No categorized files available for sorting.");
        }
        return;
    }

    const std::string base_dir = categorized_files.front().file_path;
    auto rows = get_rows();

    std::vector<std::string> files_not_moved;
    int row_index = 0;
    for (const auto& [file_name, file_type, category, subcategory] : rows) {
        try {
            const std::string effective_subcategory = subcategory.empty() ? category : subcategory;
            MovableCategorizedFile categorized_file(
                base_dir, category, effective_subcategory,
                file_name, file_type);

            categorized_file.create_cat_dirs(show_subcategory_column);
            bool moved = categorized_file.move_file(show_subcategory_column);
            update_status_column(row_index, moved);

            if (!moved) {
                files_not_moved.push_back(file_name);
                if (core_logger) {
                    core_logger->warn("File {} already exists in the destination.", file_name);
                }
            }
        } catch (const std::exception& ex) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->error("Failed to move '{}': {}", file_name, ex.what());
            }
        }
        ++row_index;
    }

    if (files_not_moved.empty()) {
        if (core_logger) {
            core_logger->info("All files have been sorted and moved successfully.");
        }
    } else if (ui_logger) {
        ui_logger->info("Categorization complete. Unmoved files: {}", files_not_moved.size());
    }

    show_close_button();
}


void CategorizationDialog::on_continue_later_button_clicked()
{
    record_categorization_to_db();
    accept();
}


void CategorizationDialog::show_close_button()
{
    if (confirm_button) {
        confirm_button->setVisible(false);
    }
    if (continue_button) {
        continue_button->setVisible(false);
    }
    if (close_button) {
        close_button->setVisible(true);
    }
}


void CategorizationDialog::update_status_column(int row, bool success)
{
    if (auto* status_item = model->item(row, 4)) {
        status_item->setText(success ? tr("Moved") : tr("Skipped"));
        status_item->setForeground(success ? QBrush(Qt::darkGreen) : QBrush(Qt::red));
    }
}


void CategorizationDialog::closeEvent(QCloseEvent* event)
{
    record_categorization_to_db();
    QDialog::closeEvent(event);
}
