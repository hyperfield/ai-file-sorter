#include "CategorizationDialog.hpp"

#include "DatabaseManager.hpp"
#include "Logger.hpp"
#include "MovableCategorizedFile.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"
#include "UndoManager.hpp"
#include "DryRunPreviewDialog.hpp"

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
#include <QMessageBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableView>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <vector>
#include <filesystem>
#include <optional>
#include <chrono>

namespace {

TestHooks::CategorizationMoveProbe& move_probe_slot() {
    static TestHooks::CategorizationMoveProbe probe;
    return probe;
}

struct ScopedFlag {
    bool& ref;
    explicit ScopedFlag(bool& target) : ref(target) { ref = true; }
    ~ScopedFlag() { ref = false; }
};

std::string to_lower_copy_str(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool contains_only_allowed_chars(const std::string& value) {
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        static const std::string forbidden = R"(<>:"/\|?*)";
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
        // Everything else is allowed (including non-ASCII letters and punctuation).
    }
    return true;
}

bool is_reserved_windows_name(const std::string& value) {
    static const std::vector<std::string> reserved = {
        "con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"
    };
    const std::string lower = to_lower_copy_str(value);
    return std::find(reserved.begin(), reserved.end(), lower) != reserved.end();
}

bool looks_like_extension_label(const std::string& value) {
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == value.size() - 1) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    if (ext.empty() || ext.size() > 5) {
        return false;
    }
    return std::all_of(ext.begin(), ext.end(), [](unsigned char ch) { return std::isalpha(ch); });
}

bool validate_labels(const std::string& category,
                     const std::string& subcategory,
                     std::string& error,
                     bool allow_identical = false) {
    constexpr size_t kMaxLabelLength = 80;
    if (category.empty() || subcategory.empty()) {
        error = "Category or subcategory is empty";
        return false;
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        error = "Category or subcategory exceeds max length";
        return false;
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        error = "Category or subcategory contains disallowed characters";
        return false;
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        error = "Category or subcategory looks like a file extension";
        return false;
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        error = "Category or subcategory is a reserved name";
        return false;
    }
    if (!allow_identical && to_lower_copy_str(category) == to_lower_copy_str(subcategory)) {
        error = "Category and subcategory are identical";
        return false;
    }
    return true;
}

std::chrono::system_clock::time_point to_system_clock(std::filesystem::file_time_type file_time) {
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    return std::chrono::clock_cast<std::chrono::system_clock>(file_time);
#else
    const auto now = decltype(file_time)::clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(file_time - now);
    return std::chrono::system_clock::now() + delta;
#endif
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
                                           const std::string& undo_dir,
                                           QWidget* parent)
    : QDialog(parent),
      db_manager(db_manager),
      show_subcategory_column(show_subcategory_col),
      core_logger(Logger::get_logger("core_logger")),
      db_logger(Logger::get_logger("db_logger")),
      ui_logger(Logger::get_logger("ui_logger")),
      undo_dir_(undo_dir)
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
    base_dir_.clear();
    dry_run_plan_.clear();
    if (!categorized_files.empty()) {
        base_dir_ = categorized_files.front().file_path;
    }
    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }
    {
        ScopedFlag guard(suppress_item_changed_);
        populate_model();
    }
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

    dry_run_checkbox = new QCheckBox(this);
    dry_run_checkbox->setChecked(false);
    layout->addWidget(dry_run_checkbox);

    model = new QStandardItemModel(this);
    model->setColumnCount(7);

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
    table_view->setColumnHidden(6, false);
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
    save_categories_button = new QPushButton(this);
    save_categories_button->setText(tr("Save Categories to Whitelist"));
    save_categories_button->setToolTip(tr("Save unique categories and subcategories to a whitelist"));
    close_button = new QPushButton(this);
    close_button->setVisible(false);

    button_layout->addWidget(save_categories_button);
    button_layout->addWidget(confirm_button);
    button_layout->addWidget(continue_button);
    button_layout->addWidget(undo_button);
    button_layout->addWidget(close_button);

    auto* tip_label = new QLabel(this);
    tip_label->setWordWrap(true);
    QFont tip_font = tip_label->font();
    tip_font.setItalic(true);
    tip_label->setFont(tip_font);
    tip_label->setText(tr("Tip: Click Category or Subcategory cells to rename them."));

    bottom_layout->addWidget(tip_label, /*stretch*/1, Qt::AlignVCenter);
    bottom_layout->addLayout(button_layout);
    layout->addLayout(bottom_layout);

    connect(confirm_button, &QPushButton::clicked, this, &CategorizationDialog::on_confirm_and_sort_button_clicked);
    connect(continue_button, &QPushButton::clicked, this, &CategorizationDialog::on_continue_later_button_clicked);
    connect(close_button, &QPushButton::clicked, this, &CategorizationDialog::accept);
    connect(undo_button, &QPushButton::clicked, this, &CategorizationDialog::on_undo_button_clicked);
    connect(save_categories_button, &QPushButton::clicked, this, &CategorizationDialog::on_save_categories_button_clicked);
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
    ScopedFlag guard(suppress_item_changed_);
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

        auto* preview_item = new QStandardItem;
        preview_item->setEditable(false);

        row << select_item << file_item << type_item << category_item << subcategory_item << status_item << preview_item;
        model->appendRow(row);
        update_preview_column(model->rowCount() - 1);
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


std::vector<std::tuple<bool, std::string, std::string, std::string>>
CategorizationDialog::get_rows() const
{
    std::vector<std::tuple<bool, std::string, std::string, std::string>> rows;
    rows.reserve(model->rowCount());

    for (int row = 0; row < model->rowCount(); ++row) {
        const bool selected = model->item(row, 0)->checkState() == Qt::Checked;
        const QString file_name = model->item(row, 1)->text();
        const QString category = model->item(row, 3)->text();
        const QString subcategory = show_subcategory_column
                                        ? model->item(row, 4)->text()
                                        : QString();
        rows.emplace_back(selected,
                          file_name.toStdString(),
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
    dry_run_plan_.clear();
    auto rows = get_rows();

    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }

    const bool dry_run = dry_run_checkbox && dry_run_checkbox->isChecked();
    if (dry_run && core_logger) {
        core_logger->info("Dry run enabled; will not move files.");
    }

    std::vector<std::string> files_not_moved;
    ScopedFlag guard(suppress_item_changed_);
    int row_index = 0;
    for (const auto& [selected, file_name, category, subcategory] : rows) {
        if (!selected) {
            update_status_column(row_index, false, false);
            ++row_index;
            continue;
        }
        handle_selected_row(row_index,
                            file_name,
                            category,
                            subcategory,
                            base_dir,
                            files_not_moved,
                            dry_run);
        ++row_index;
    }

    if (files_not_moved.empty()) {
        if (core_logger) {
            core_logger->info("All files have been sorted and moved successfully.");
        }
    } else if (ui_logger) {
        ui_logger->info("Categorization complete. Unmoved files: {}", files_not_moved.size());
    }

    if (dry_run) {
        // Show preview dialog of planned moves.
        std::vector<DryRunPreviewDialog::Entry> entries;
        entries.reserve(static_cast<size_t>(model->rowCount()));
        for (int row = 0; row < model->rowCount(); ++row) {
            if (auto* select_item = model->item(row, 0)) {
                if (select_item->checkState() != Qt::Checked) {
                    continue;
                }
            }
            const auto* file_item = model->item(row, 1);
            const auto* cat_item = model->item(row, 3);
            const auto* sub_item = model->item(row, 4);
            if (!file_item || !cat_item) {
                continue;
            }
            const std::string file_name = file_item->text().toStdString();
            const std::string category = cat_item->text().toStdString();
            const std::string subcategory = show_subcategory_column && sub_item
                                                ? sub_item->text().toStdString()
                                                : std::string();
            std::string debug_reason;
            auto rec = build_preview_record_for_row(row, &debug_reason);
            if (!rec) {
                if (core_logger) {
                    core_logger->warn("Dry run preview skipped row {}: {}", row, debug_reason);
                }
                continue;
            }
            std::string to_label = rec->category;
#ifdef _WIN32
            const char sep = '\\\\';
#else
            const char sep = '/';
#endif
            if (rec->use_subcategory && !rec->subcategory.empty()) {
                to_label += std::string(1, sep) + rec->subcategory;
            }
            to_label += std::string(1, sep) + rec->file_name;

            std::string destination = rec->destination;
#ifdef _WIN32
            std::replace(destination.begin(), destination.end(), '/', '\\');
#endif
            std::string source_tooltip = rec->source;
#ifdef _WIN32
            std::replace(source_tooltip.begin(), source_tooltip.end(), '/', '\\');
#endif

            entries.push_back(DryRunPreviewDialog::Entry{
                /*from_label*/ rec->file_name,
                /*to_label*/ to_label,
                /*source_tooltip*/ source_tooltip,
                /*destination_tooltip*/ destination});
        }
        if (core_logger) {
            core_logger->info("Dry run preview entries built: {}", entries.size());
        }
        DryRunPreviewDialog preview_dialog(entries, this);
        preview_dialog.exec();

        // In preview mode, keep the dialog actionable so the user can uncheck Dry run and re-run.
        if (undo_button) {
            undo_button->setVisible(false);
            undo_button->setEnabled(false);
        }
        restore_action_buttons();
        return;
    }

    if (!move_history_.empty() && undo_button) {
        undo_button->setVisible(true);
        undo_button->setEnabled(true);
    }

    if (!move_history_.empty()) {
        persist_move_plan();
    }

    show_close_button();
}

void CategorizationDialog::handle_selected_row(int row_index,
                                               const std::string& file_name,
                                               const std::string& category,
                                               const std::string& subcategory,
                                               const std::string& base_dir,
                                               std::vector<std::string>& files_not_moved,
                                               bool dry_run)
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

    std::string validation_error;
    const bool allow_identical = !show_subcategory_column;
    if (!validate_labels(category, effective_subcategory, validation_error, allow_identical)) {
        update_status_column(row_index, false);
        files_not_moved.push_back(file_name);
        if (core_logger) {
            core_logger->warn("Skipping move for '{}' due to invalid category/subcategory: {} (cat='{}', sub='{}')",
                              file_name,
                              validation_error,
                              category,
                              effective_subcategory);
        }
        return;
    }

    try {
        MovableCategorizedFile categorized_file(
            base_dir, category, effective_subcategory, file_name);

        const auto preview_paths = categorized_file.preview_move_paths(show_subcategory_column);

        if (dry_run) {
            set_preview_status(row_index, preview_paths.destination);
            dry_run_plan_.push_back(PreviewRecord{
                preview_paths.source,
                preview_paths.destination,
                file_name,
                category,
                effective_subcategory,
                show_subcategory_column});
            if (core_logger) {
                core_logger->info("Dry run: would move '{}' to '{}'",
                                  preview_paths.source,
                                  preview_paths.destination);
            }
            return;
        }

        categorized_file.create_cat_dirs(show_subcategory_column);
        bool moved = categorized_file.move_file(show_subcategory_column);
        update_status_column(row_index, moved);

        if (!moved) {
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("File {} already exists in the destination.", file_name);
            }
        } else {
            std::error_code ec;
            const std::uintmax_t size_bytes = std::filesystem::file_size(Utils::utf8_to_path(preview_paths.destination), ec);
            std::time_t mtime_value = 0;
            if (!ec) {
                const auto ftime = std::filesystem::last_write_time(Utils::utf8_to_path(preview_paths.destination), ec);
                if (!ec) {
                    const auto sys = to_system_clock(ftime);
                    mtime_value = std::chrono::system_clock::to_time_t(sys);
                }
            }
            record_move_for_undo(row_index, preview_paths.source, preview_paths.destination, size_bytes, mtime_value);
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

void CategorizationDialog::on_save_categories_button_clicked()
{
    if (!save_categories_callback_) {
        QMessageBox::information(this, tr("Save Categories"),
                               tr("Category saving is not configured."));
        return;
    }
    
    // Extract unique categories and subcategories from current model
    std::set<std::string> categories_set;
    std::set<std::string> subcategories_set;
    
    for (int row = 0; row < model->rowCount(); ++row) {
        auto* category_item = model->item(row, 3);
        if (category_item) {
            std::string category = category_item->text().toStdString();
            if (!category.empty()) {
                categories_set.insert(category);
            }
        }
        
        if (show_subcategory_column) {
            auto* subcategory_item = model->item(row, 4);
            if (subcategory_item) {
                std::string subcategory = subcategory_item->text().toStdString();
                if (!subcategory.empty()) {
                    subcategories_set.insert(subcategory);
                }
            }
        }
    }
    
    std::vector<std::string> categories(categories_set.begin(), categories_set.end());
    std::vector<std::string> subcategories(subcategories_set.begin(), subcategories_set.end());
    
    // Show confirmation with counts
    QString msg = tr("Save %1 unique categories and %2 unique subcategories to whitelist?")
                    .arg(categories.size())
                    .arg(subcategories.size());
    
    auto reply = QMessageBox::question(this, tr("Confirm Save"),
                                      msg,
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        save_categories_callback_(categories, subcategories);
        QMessageBox::information(this, tr("Categories Saved"),
                               tr("Categories have been saved to the whitelist."));
    }
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

void CategorizationDialog::record_move_for_undo(int row,
                                                const std::string& source,
                                                const std::string& destination,
                                                std::uintmax_t size_bytes,
                                                std::time_t mtime)
{
    move_history_.push_back(MoveRecord{row, source, destination, size_bytes, mtime});
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
        update_preview_column(row);
    }
    updating_select_all = false;
    update_select_all_state();
}

void CategorizationDialog::on_show_subcategories_toggled(bool checked)
{
    show_subcategory_column = checked;
    apply_subcategory_visibility();
    for (int row = 0; row < model->rowCount(); ++row) {
        update_preview_column(row);
    }
}

void CategorizationDialog::apply_subcategory_visibility()
{
    if (table_view) {
        table_view->setColumnHidden(4, !show_subcategory_column);
        table_view->setColumnHidden(6, false);
    }
}

std::optional<std::string> CategorizationDialog::compute_preview_path(int row) const
{
    auto rec = build_preview_record_for_row(row);
    if (rec) {
        return rec->destination;
    }
    return std::nullopt;
}

std::optional<CategorizationDialog::PreviewRecord>
CategorizationDialog::build_preview_record_for_row(int row, std::string* debug_reason) const
{
    auto fail = [&](std::string reason) -> std::optional<PreviewRecord> {
        if (debug_reason) {
            *debug_reason = std::move(reason);
        }
        return std::nullopt;
    };

    if (!model || row < 0 || row >= model->rowCount()) {
        return fail("Invalid model or row");
    }
    if (base_dir_.empty()) {
        return fail("Base dir empty");
    }

    const auto* file_item = model->item(row, 1);
    const auto* category_item = model->item(row, 3);
    const auto* subcategory_item = model->item(row, 4);
    if (!file_item || !category_item) {
        return fail("Missing file/category item");
    }

    const std::string file_name = file_item->text().toStdString();
    const std::string category = category_item->text().toStdString();
    const std::string subcategory = show_subcategory_column && subcategory_item
        ? subcategory_item->text().toStdString()
        : std::string();
    const std::string effective_subcategory = subcategory.empty() ? category : subcategory;

    std::string validation_error;
    const bool allow_identical = !show_subcategory_column;
    if (!validate_labels(category, effective_subcategory, validation_error, allow_identical)) {
        return fail("Validation failed: " + validation_error);
    }

    try {
        MovableCategorizedFile categorized_file(base_dir_, category, effective_subcategory, file_name);
        const auto preview_paths = categorized_file.preview_move_paths(show_subcategory_column);
        return PreviewRecord{
            preview_paths.source,
            preview_paths.destination,
            file_name,
            category,
            effective_subcategory,
            show_subcategory_column};
    } catch (...) {
        return fail("Exception building preview record");
    }
}

void CategorizationDialog::update_preview_column(int row)
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return;
    }
    auto* preview_item = model->item(row, 6);
    if (!preview_item) {
        return;
    }
    const auto preview = compute_preview_path(row);
    if (preview) {
        std::string display = *preview;
#ifdef _WIN32
        std::replace(display.begin(), display.end(), '/', '\\');
#endif
        preview_item->setText(QString::fromStdString(display));
        preview_item->setToolTip(QString::fromStdString(display));
    } else {
        preview_item->setText(QStringLiteral("-"));
        preview_item->setToolTip(QString());
    }
}

void CategorizationDialog::set_preview_status(int row, const std::string& destination)
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return;
    }
    if (auto* status_item = model->item(row, 5)) {
        status_item->setData(static_cast<int>(RowStatus::Preview), kStatusRole);
        status_item->setText(tr("Preview"));
        status_item->setForeground(QBrush(Qt::blue));
        std::string display = destination;
#ifdef _WIN32
        std::replace(display.begin(), display.end(), '/', '\\');
#endif
        status_item->setToolTip(QString::fromStdString(display));
    }
}

void CategorizationDialog::persist_move_plan()
{
    if (undo_dir_.empty() || base_dir_.empty() || move_history_.empty()) {
        return;
    }

    std::vector<UndoManager::Entry> entries;
    entries.reserve(move_history_.size());
    for (const auto& rec : move_history_) {
        entries.push_back(UndoManager::Entry{
            rec.source_path,
            rec.destination_path,
            rec.size_bytes,
            rec.mtime});
    }

    UndoManager manager(undo_dir_);
    manager.save_plan(base_dir_, entries, core_logger);
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
    set_text_if(dry_run_checkbox, tr("Dry run (preview only, do not move files)"));
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
            tr("Status"),
            tr("Planned destination")
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
    case RowStatus::Preview:
        item->setText(tr("Preview"));
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
    case RowStatus::Preview:
        return status;
    }

    return RowStatus::None;
}


void CategorizationDialog::on_item_changed(QStandardItem* item)
{
    if (!item || updating_select_all || suppress_item_changed_) {
        return;
    }

    if (item->column() == 0) {
        update_select_all_state();
    } else if (item->column() == 3 || item->column() == 4) {
        update_preview_column(item->row());
    }
    // invalidate preview plan only on user-facing edits (selection/category fields)
    if (item->column() == 0 || item->column() == 3 || item->column() == 4) {
        dry_run_plan_.clear();
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
        for (int row = 0; row < model->rowCount(); ++row) {
            update_preview_column(row);
        }
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
