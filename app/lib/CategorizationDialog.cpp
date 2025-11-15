#include "CategorizationDialog.hpp"

#include "DatabaseManager.hpp"
#include "Logger.hpp"
#include "MovableCategorizedFile.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"

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
#include <QIcon>
#include <QLabel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableView>
#include <QVBoxLayout>
#include <QSignalBlocker>

#include <fmt/format.h>

#include <filesystem>

namespace {

TestHooks::CategorizationMoveProbe& move_probe_slot() {
    static TestHooks::CategorizationMoveProbe probe;
    return probe;
}

} // namespace

namespace TestHooks {

void set_categorization_move_probe(CategorizationMoveProbe probe) {
    move_probe_slot() = std::move(probe);
}

void reset_categorization_move_probe() {
    move_probe_slot() = CategorizationMoveProbe{};
}

} // namespace TestHooks

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
    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }
    populate_model();
    exec();
}


void CategorizationDialog::setup_ui()
{
    auto* layout = new QVBoxLayout(this);

    select_all_checkbox = new QCheckBox(this);
    select_all_checkbox->setChecked(true);
    layout->addWidget(select_all_checkbox);

    show_subcategories_checkbox = new QCheckBox(this);
    show_subcategories_checkbox->setChecked(show_subcategory_column);
    layout->addWidget(show_subcategories_checkbox);

    model = new QStandardItemModel(this);
    model->setColumnCount(6);

    table_view = new QTableView(this);
    table_view->setModel(model);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    table_view->horizontalHeader()->setStretchLastSection(true);
    table_view->verticalHeader()->setVisible(false);
    table_view->horizontalHeader()->setSectionsClickable(true);
    table_view->horizontalHeader()->setSortIndicatorShown(true);
    table_view->setSortingEnabled(true);
    table_view->setColumnHidden(2, false);
    table_view->setColumnHidden(4, !show_subcategory_column);
    table_view->setColumnWidth(0, 70);
    table_view->setIconSize(QSize(16, 16));
    table_view->setColumnWidth(2, table_view->iconSize().width() + 12);
    layout->addWidget(table_view, 1);

    auto* bottom_layout = new QHBoxLayout();
    bottom_layout->setContentsMargins(0, 0, 0, 0);
    bottom_layout->setSpacing(8);
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);

    confirm_button = new QPushButton(this);
    continue_button = new QPushButton(this);
    undo_button = new QPushButton(this);
    undo_button->setEnabled(false);
    undo_button->setVisible(false);
    close_button = new QPushButton(this);
    close_button->setVisible(false);

    button_layout->addWidget(confirm_button);
    button_layout->addWidget(continue_button);
    button_layout->addWidget(undo_button);
    button_layout->addWidget(close_button);

    auto* tip_label = new QLabel(this);
    tip_label->setWordWrap(true);
    QFont tip_font = tip_label->font();
    tip_font.setItalic(true);
    tip_label->setFont(tip_font);
    tip_label->setText(tr("Tip: Click Category or Subcategory cells (pencil icon) to rename them."));

    bottom_layout->addWidget(tip_label, /*stretch*/1, Qt::AlignVCenter);
    bottom_layout->addLayout(button_layout);
    layout->addLayout(bottom_layout);

    connect(confirm_button, &QPushButton::clicked, this, &CategorizationDialog::on_confirm_and_sort_button_clicked);
    connect(continue_button, &QPushButton::clicked, this, &CategorizationDialog::on_continue_later_button_clicked);
    connect(close_button, &QPushButton::clicked, this, &CategorizationDialog::accept);
    connect(undo_button, &QPushButton::clicked, this, &CategorizationDialog::on_undo_button_clicked);
    connect(select_all_checkbox, &QCheckBox::toggled, this, &CategorizationDialog::on_select_all_toggled);
    connect(model, &QStandardItemModel::itemChanged, this, &CategorizationDialog::on_item_changed);
    connect(show_subcategories_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_show_subcategories_toggled);
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

QIcon edit_icon()
{
    QIcon icon = QIcon::fromTheme(QStringLiteral("edit-rename"));
    if (!icon.isNull()) {
        return icon;
    }
    icon = QIcon::fromTheme(QStringLiteral("document-edit"));
    if (!icon.isNull()) {
        return icon;
    }
    if (auto* style = QApplication::style()) {
        return style->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    return QIcon();
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
        category_item->setIcon(edit_icon());

        auto* subcategory_item = new QStandardItem(QString::fromStdString(file.subcategory));
        subcategory_item->setEditable(true);
        subcategory_item->setIcon(edit_icon());

        auto* status_item = new QStandardItem;
        status_item->setEditable(false);
        status_item->setData(static_cast<int>(RowStatus::None), kStatusRole);
        apply_status_text(status_item);
        status_item->setForeground(QBrush());

        row << select_item << file_item << type_item << category_item << subcategory_item << status_item;
        model->appendRow(row);
    }

    updating_select_all = false;
    apply_subcategory_visibility();
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
            entry.file_name, file_type, entry.file_path, resolved, entry.used_consistency_hints);

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

    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }

    std::vector<std::string> files_not_moved;
    int row_index = 0;
    for (const auto& [selected, file_name, file_type, category, subcategory] : rows) {
        if (!selected) {
            update_status_column(row_index, false, false);
            ++row_index;
            continue;
        }
        handle_selected_row(row_index,
                            file_name,
                            file_type,
                            category,
                            subcategory,
                            base_dir,
                            files_not_moved);
        ++row_index;
    }

    if (files_not_moved.empty()) {
        if (core_logger) {
            core_logger->info("All files have been sorted and moved successfully.");
        }
    } else if (ui_logger) {
        ui_logger->info("Categorization complete. Unmoved files: {}", files_not_moved.size());
    }

    if (!move_history_.empty() && undo_button) {
        undo_button->setVisible(true);
        undo_button->setEnabled(true);
    }

    show_close_button();
}

void CategorizationDialog::handle_selected_row(int row_index,
                                               const std::string& file_name,
                                               const std::string& file_type,
                                               const std::string& category,
                                               const std::string& subcategory,
                                               const std::string& base_dir,
                                               std::vector<std::string>& files_not_moved)
{
    const std::string effective_subcategory = subcategory.empty() ? category : subcategory;

    if (auto& probe = move_probe_slot()) {
        probe(TestHooks::CategorizationMoveInfo{
            show_subcategory_column,
            category,
            effective_subcategory,
            file_name
        });
        update_status_column(row_index, true);
        return;
    }

    try {
        MovableCategorizedFile categorized_file(
            base_dir, category, effective_subcategory,
            file_name, file_type);

        const auto preview_paths = categorized_file.preview_move_paths(show_subcategory_column);

        categorized_file.create_cat_dirs(show_subcategory_column);
        bool moved = categorized_file.move_file(show_subcategory_column);
        update_status_column(row_index, moved);

        if (!moved) {
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("File {} already exists in the destination.", file_name);
            }
        } else {
            record_move_for_undo(row_index, preview_paths.source, preview_paths.destination);
        }
    } catch (const std::exception& ex) {
        update_status_column(row_index, false);
        files_not_moved.push_back(file_name);
        if (core_logger) {
            core_logger->error("Failed to move '{}': {}", file_name, ex.what());
        }
    }
}


void CategorizationDialog::on_continue_later_button_clicked()
{
    record_categorization_to_db();
    accept();
}

void CategorizationDialog::on_undo_button_clicked()
{
    if (!undo_move_history()) {
        return;
    }

    update_status_after_undo();
    restore_action_buttons();
    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }
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

void CategorizationDialog::restore_action_buttons()
{
    if (confirm_button) {
        confirm_button->setVisible(true);
    }
    if (continue_button) {
        continue_button->setVisible(true);
    }
    if (close_button) {
        close_button->setVisible(false);
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

void CategorizationDialog::record_move_for_undo(int row, const std::string& source, const std::string& destination)
{
    move_history_.push_back(MoveRecord{row, source, destination});
}

void CategorizationDialog::remove_empty_parent_directories(const std::string& destination)
{
    std::filesystem::path dest_path = Utils::utf8_to_path(destination);
    auto parent = dest_path.parent_path();
    while (!parent.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(parent)) {
            parent = parent.parent_path();
            continue;
        }
        if (std::filesystem::is_directory(parent) &&
            std::filesystem::is_empty(parent, ec) && !ec) {
            std::filesystem::remove(parent, ec);
            parent = parent.parent_path();
        } else {
            break;
        }
    }
}

bool CategorizationDialog::move_file_back(const std::string& source, const std::string& destination)
{
    std::error_code ec;
    auto destination_path = Utils::utf8_to_path(destination);
    auto source_path = Utils::utf8_to_path(source);

    if (!std::filesystem::exists(destination_path)) {
        if (core_logger) {
            core_logger->warn("Undo skipped; destination '{}' missing", destination);
        }
        return false;
    }

    std::filesystem::create_directories(source_path.parent_path(), ec);

    try {
        std::filesystem::rename(destination_path, source_path);
    } catch (const std::filesystem::filesystem_error& ex) {
        if (core_logger) {
            core_logger->error("Undo move failed '{}' -> '{}': {}", destination, source, ex.what());
        }
        return false;
    }

    remove_empty_parent_directories(destination);
    return true;
}

bool CategorizationDialog::undo_move_history()
{
    if (move_history_.empty()) {
        return false;
    }

    bool any_success = false;
    for (auto it = move_history_.rbegin(); it != move_history_.rend(); ++it) {
        if (move_file_back(it->source_path, it->destination_path)) {
            any_success = true;
        }
    }

    if (any_success && core_logger) {
        core_logger->info("Undo completed for {} moved file(s)", move_history_.size());
    }

    return any_success;
}

void CategorizationDialog::update_status_after_undo()
{
    for (const auto& record : move_history_) {
        update_status_column(record.row_index, false, false);
    }
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

void CategorizationDialog::on_show_subcategories_toggled(bool checked)
{
    show_subcategory_column = checked;
    apply_subcategory_visibility();
}

void CategorizationDialog::apply_subcategory_visibility()
{
    if (table_view) {
        table_view->setColumnHidden(4, !show_subcategory_column);
    }
}

void CategorizationDialog::clear_move_history()
{
    move_history_.clear();
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
    set_text_if(show_subcategories_checkbox, tr("Create subcategory folders"));
    set_text_if(confirm_button, tr("Confirm and Sort"));
    set_text_if(continue_button, tr("Continue Later"));
    set_text_if(undo_button, tr("Undo this change"));
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
void CategorizationDialog::set_show_subcategory_column(bool enabled)
{
    if (show_subcategory_column == enabled) {
        return;
    }
    show_subcategory_column = enabled;
    if (show_subcategories_checkbox) {
        QSignalBlocker blocker(show_subcategories_checkbox);
        show_subcategories_checkbox->setChecked(enabled);
    }
    apply_subcategory_visibility();
}
#ifdef AI_FILE_SORTER_TEST_BUILD
void CategorizationDialog::test_set_entries(const std::vector<CategorizedFile>& files) {
    categorized_files = files;
    populate_model();
}

void CategorizationDialog::test_trigger_confirm() {
    on_confirm_and_sort_button_clicked();
}

void CategorizationDialog::test_trigger_undo() {
    on_undo_button_clicked();
}

bool CategorizationDialog::test_undo_enabled() const {
    return undo_button && undo_button->isEnabled();
}
#endif
