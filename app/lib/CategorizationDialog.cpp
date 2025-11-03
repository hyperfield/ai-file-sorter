#include "CategorizationDialog.hpp"

#include "DatabaseManager.hpp"
#include "Logger.hpp"
#include "MovableCategorizedFile.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QStyle>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableView>
#include <QVBoxLayout>
#include <QSignalBlocker>

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
    resize(1100, 720);
    setup_ui();
    retranslate_ui();
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

    select_all_checkbox = new QCheckBox(this);
    select_all_checkbox->setChecked(true);
    layout->addWidget(select_all_checkbox);

    model = new QStandardItemModel(this);
    model->setColumnCount(6);

    table_view = new QTableView(this);
    table_view->setModel(model);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    table_view->horizontalHeader()->setStretchLastSection(true);
    table_view->verticalHeader()->setVisible(false);
    table_view->setColumnHidden(2, false);
    table_view->setColumnHidden(4, !show_subcategory_column);
    table_view->setColumnWidth(0, 70);
    table_view->setIconSize(QSize(16, 16));
    table_view->setColumnWidth(2, table_view->iconSize().width() + 12);
    layout->addWidget(table_view, 1);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);

    confirm_button = new QPushButton(this);
    continue_button = new QPushButton(this);
    close_button = new QPushButton(this);
    close_button->setVisible(false);

    button_layout->addWidget(confirm_button);
    button_layout->addWidget(continue_button);
    button_layout->addWidget(close_button);

    layout->addLayout(button_layout);

    connect(confirm_button, &QPushButton::clicked, this, &CategorizationDialog::on_confirm_and_sort_button_clicked);
    connect(continue_button, &QPushButton::clicked, this, &CategorizationDialog::on_continue_later_button_clicked);
    connect(close_button, &QPushButton::clicked, this, &CategorizationDialog::accept);
    connect(select_all_checkbox, &QCheckBox::toggled, this, &CategorizationDialog::on_select_all_toggled);
    connect(model, &QStandardItemModel::itemChanged, this, &CategorizationDialog::on_item_changed);
}


namespace {
QIcon type_icon(const QString& code)
{
    if (auto* style = QApplication::style()) {
        return code == QStringLiteral("D")
                   ? style->standardIcon(QStyle::SP_DirIcon)
                   : style->standardIcon(QStyle::SP_FileIcon);
    }
    return {};
}
}

void CategorizationDialog::populate_model()
{
    model->removeRows(0, model->rowCount());

    const int type_col_width = table_view ? table_view->iconSize().width() + 12 : 28;
    if (table_view) {
        table_view->setColumnWidth(2, type_col_width);
    }

    updating_select_all = true;

    for (const auto& file : categorized_files) {
        QList<QStandardItem*> row;

        auto* select_item = new QStandardItem;
        select_item->setCheckable(true);
        select_item->setCheckState(Qt::Checked);
        select_item->setEditable(false);

        auto* file_item = new QStandardItem(QString::fromStdString(file.file_name));
        file_item->setEditable(false);
        file_item->setData(QString::fromStdString(file.file_path), Qt::UserRole + 1);

        auto* type_item = new QStandardItem;
        type_item->setEditable(false);
        type_item->setData(file.type == FileType::Directory ? QStringLiteral("D") : QStringLiteral("F"), Qt::UserRole);
        type_item->setTextAlignment(Qt::AlignCenter);
        update_type_icon(type_item);

        auto* category_item = new QStandardItem(QString::fromStdString(file.category));
        category_item->setEditable(true);

        auto* subcategory_item = new QStandardItem(QString::fromStdString(file.subcategory));
        subcategory_item->setEditable(true);

        auto* status_item = new QStandardItem;
        status_item->setEditable(false);
        status_item->setData(static_cast<int>(RowStatus::None), kStatusRole);
        apply_status_text(status_item);
        status_item->setForeground(QBrush());

        row << select_item << file_item << type_item << category_item << subcategory_item << status_item;
        model->appendRow(row);
    }

    updating_select_all = false;
    table_view->setColumnHidden(4, !show_subcategory_column);
    table_view->resizeColumnsToContents();
    update_select_all_state();
}

void CategorizationDialog::update_type_icon(QStandardItem* item)
{
    if (!item) {
        return;
    }

    const QString code = item->data(Qt::UserRole).toString();
    item->setIcon(type_icon(code));
    item->setText(QString());
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
        std::string category = model->item(row, 3)->text().toStdString();
        std::string subcategory = show_subcategory_column
                                      ? model->item(row, 4)->text().toStdString()
                                      : "";

        auto resolved = db_manager->resolve_category(category, subcategory);

        const std::string file_type = (entry.type == FileType::Directory) ? "D" : "F";
        db_manager->insert_or_update_file_with_categorization(
            entry.file_name, file_type, entry.file_path, resolved);

        entry.category = resolved.category;
        entry.subcategory = resolved.subcategory;
        entry.taxonomy_id = resolved.taxonomy_id;

        model->item(row, 3)->setText(QString::fromStdString(resolved.category));
        if (show_subcategory_column) {
            model->item(row, 4)->setText(QString::fromStdString(resolved.subcategory));
        }
    }
}


std::vector<std::tuple<bool, std::string, std::string, std::string, std::string>>
CategorizationDialog::get_rows() const
{
    std::vector<std::tuple<bool, std::string, std::string, std::string, std::string>> rows;
    rows.reserve(model->rowCount());

    for (int row = 0; row < model->rowCount(); ++row) {
        const bool selected = model->item(row, 0)->checkState() == Qt::Checked;
        const QString file_name = model->item(row, 1)->text();
        const QString file_type = model->item(row, 2)->data(Qt::UserRole).toString();
        const QString category = model->item(row, 3)->text();
        const QString subcategory = show_subcategory_column
                                        ? model->item(row, 4)->text()
                                        : QString();
        rows.emplace_back(selected,
                          file_name.toStdString(),
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
    for (const auto& [selected, file_name, file_type, category, subcategory] : rows) {
        if (!selected) {
            update_status_column(row_index, false, false);
            ++row_index;
            continue;
        }
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


void CategorizationDialog::update_status_column(int row, bool success, bool attempted)
{
    if (auto* status_item = model->item(row, 5)) {
        RowStatus status = RowStatus::None;
        if (!attempted) {
            status = RowStatus::NotSelected;
            status_item->setForeground(QBrush(Qt::gray));
        } else if (success) {
            status = RowStatus::Moved;
            status_item->setForeground(QBrush(Qt::darkGreen));
        } else {
            status = RowStatus::Skipped;
            status_item->setForeground(QBrush(Qt::red));
        }

        if (status == RowStatus::None) {
            status_item->setForeground(QBrush());
        }

        status_item->setData(static_cast<int>(status), kStatusRole);
        apply_status_text(status_item);
    }
}


void CategorizationDialog::on_select_all_toggled(bool checked)
{
    apply_select_all(checked);
}


void CategorizationDialog::apply_select_all(bool checked)
{
    updating_select_all = true;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (auto* item = model->item(row, 0)) {
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    }
    updating_select_all = false;
    update_select_all_state();
}

void CategorizationDialog::retranslate_ui()
{
    setWindowTitle(tr("Review Categorization"));

    const auto set_text_if = [](auto* widget, const QString& text) {
        if (widget) {
            widget->setText(text);
        }
    };

    set_text_if(select_all_checkbox, tr("Select all"));
    set_text_if(confirm_button, tr("Confirm and Sort"));
    set_text_if(continue_button, tr("Continue Later"));
    set_text_if(close_button, tr("Close"));

    if (model) {
        model->setHorizontalHeaderLabels(QStringList{
            tr("Move"),
            tr("File"),
            tr("Type"),
            tr("Category"),
            tr("Subcategory"),
            tr("Status")
        });

        for (int row = 0; row < model->rowCount(); ++row) {
            if (auto* type_item = model->item(row, 2)) {
                update_type_icon(type_item);
                type_item->setTextAlignment(Qt::AlignCenter);
            }
            if (auto* status_item = model->item(row, 5)) {
                apply_status_text(status_item);
            }
        }
    }
}

void CategorizationDialog::apply_status_text(QStandardItem* item) const
{
    if (!item) {
        return;
    }

    switch (status_from_item(item)) {
    case RowStatus::Moved:
        item->setText(tr("Moved"));
        break;
    case RowStatus::Skipped:
        item->setText(tr("Skipped"));
        break;
    case RowStatus::NotSelected:
        item->setText(tr("Not selected"));
        break;
    case RowStatus::None:
    default:
        item->setText(QString());
        break;
    }
}

CategorizationDialog::RowStatus CategorizationDialog::status_from_item(const QStandardItem* item) const
{
    if (!item) {
        return RowStatus::None;
    }

    bool ok = false;
    const int value = item->data(kStatusRole).toInt(&ok);
    if (!ok) {
        return RowStatus::None;
    }

    const RowStatus status = static_cast<RowStatus>(value);
    switch (status) {
    case RowStatus::None:
    case RowStatus::Moved:
    case RowStatus::Skipped:
    case RowStatus::NotSelected:
        return status;
    }

    return RowStatus::None;
}


void CategorizationDialog::on_item_changed(QStandardItem* item)
{
    if (!item || updating_select_all) {
        return;
    }

    if (item->column() == 0) {
        update_select_all_state();
    }
}


void CategorizationDialog::update_select_all_state()
{
    if (!select_all_checkbox) {
        return;
    }

    bool all_checked = true;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (auto* item = model->item(row, 0)) {
            if (item->checkState() != Qt::Checked) {
                all_checked = false;
                break;
            }
        }
    }

    QSignalBlocker blocker(select_all_checkbox);
    select_all_checkbox->setChecked(all_checked);
}

void CategorizationDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange) {
        retranslate_ui();
    }
}


void CategorizationDialog::closeEvent(QCloseEvent* event)
{
    record_categorization_to_db();
    QDialog::closeEvent(event);
}
