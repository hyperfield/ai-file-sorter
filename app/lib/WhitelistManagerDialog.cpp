#include "WhitelistManagerDialog.hpp"

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QLayout>
#include <QScrollArea>

#include <unordered_map>
#include <utility>

namespace {
QString join_lines(const std::vector<std::string>& items) {
    QStringList list;
    for (const auto& i : items) {
        list << QString::fromStdString(i);
    }
    return list.join(", ");
}

std::vector<std::string> split_lines(const QString& text) {
    std::vector<std::string> items;
    for (const auto& part : text.split(",")) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            items.emplace_back(trimmed.toStdString());
        }
    }
    return items;
}

void clear_layout(QLayout& layout)
{
    while (auto* item = layout.takeAt(0)) {
        if (auto* child_layout = item->layout()) {
            clear_layout(*child_layout);
            delete child_layout;
        }
        if (auto* widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
}

struct EditDialogResult {
    QString name;
    std::vector<std::string> categories;
    std::vector<std::string> subcategories;
    std::unordered_map<std::string, std::vector<std::string>> subcategories_by_category;
};

using CategoryRowEdits = std::unordered_map<std::string, QLineEdit*>;

std::unordered_map<std::string, std::vector<std::string>> collect_category_row_values(
    const CategoryRowEdits& row_edits)
{
    std::unordered_map<std::string, std::vector<std::string>> values;
    for (const auto& [category, edit] : row_edits) {
        if (!edit) {
            continue;
        }
        auto subcategories = split_lines(edit->text());
        if (!subcategories.empty()) {
            values[category] = std::move(subcategories);
        }
    }
    return values;
}

bool has_category_row_values(const CategoryRowEdits& row_edits)
{
    for (const auto& [category, edit] : row_edits) {
        (void)category;
        if (edit && !split_lines(edit->text()).empty()) {
            return true;
        }
    }
    return false;
}

std::optional<EditDialogResult> show_edit_dialog(QWidget* parent,
                                                 const QString& initial_name,
                                                 const WhitelistEntry& entry)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Edit whitelist"));
    auto* layout = new QVBoxLayout(&dialog);
    const auto add_label = [&](const QString& text) {
        auto* label = new QLabel(text, &dialog);
        label->setWordWrap(true);
        layout->addWidget(label);
    };

    auto* name_edit = new QLineEdit(&dialog);
    name_edit->setText(initial_name);
    add_label(QObject::tr("Name:"));
    layout->addWidget(name_edit);

    auto* cats_edit = new QTextEdit(&dialog);
    cats_edit->setPlainText(join_lines(entry.categories));
    add_label(QObject::tr("Main categories / top-level folders (comma separated):"));
    layout->addWidget(cats_edit);

    auto* subs_edit = new QTextEdit(&dialog);
    subs_edit->setPlainText(join_lines(entry.subcategories));
    add_label(QObject::tr("Global subcategories (optional; active when category-specific rows are empty):"));
    layout->addWidget(subs_edit);

    add_label(QObject::tr("Category-specific subcategories (optional; active when global subcategories are empty):"));
    auto* mapped_subs_container = new QWidget(&dialog);
    auto* mapped_subs_layout = new QFormLayout(mapped_subs_container);
    mapped_subs_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto* mapped_subs_scroll = new QScrollArea(&dialog);
    mapped_subs_scroll->setWidgetResizable(true);
    mapped_subs_scroll->setWidget(mapped_subs_container);
    mapped_subs_scroll->setMinimumHeight(120);
    layout->addWidget(mapped_subs_scroll);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    std::unordered_map<std::string, std::vector<std::string>> mapped_values =
        entry.subcategories_by_category;
    CategoryRowEdits row_edits;
    bool rebuilding_rows = false;

    const auto update_exclusive_mode = [&]() {
        if (rebuilding_rows) {
            return;
        }
        const bool has_global = !split_lines(subs_edit->toPlainText()).empty();
        const bool has_specific = has_category_row_values(row_edits);
        const bool use_specific = has_specific;
        const bool use_global = has_global && !use_specific;
        subs_edit->setEnabled(!use_specific);
        mapped_subs_scroll->setEnabled(!use_global);
    };

    const auto remember_rows = [&]() {
        for (const auto& [category, subcategories] : collect_category_row_values(row_edits)) {
            mapped_values[category] = subcategories;
        }
        for (const auto& [category, edit] : row_edits) {
            if (edit && split_lines(edit->text()).empty()) {
                mapped_values.erase(category);
            }
        }
    };

    const auto rebuild_category_rows = [&]() {
        rebuilding_rows = true;
        remember_rows();
        clear_layout(*mapped_subs_layout);
        row_edits.clear();

        for (const auto& category : split_lines(cats_edit->toPlainText())) {
            auto* row_edit = new QLineEdit(mapped_subs_container);
            if (auto existing = mapped_values.find(category); existing != mapped_values.end()) {
                row_edit->setText(join_lines(existing->second));
            }
            QObject::connect(row_edit, &QLineEdit::textChanged, &dialog, update_exclusive_mode);
            mapped_subs_layout->addRow(QString::fromStdString(category) + QStringLiteral(":"), row_edit);
            row_edits[category] = row_edit;
        }

        rebuilding_rows = false;
        update_exclusive_mode();
    };

    QObject::connect(cats_edit, &QTextEdit::textChanged, &dialog, rebuild_category_rows);
    QObject::connect(subs_edit, &QTextEdit::textChanged, &dialog, update_exclusive_mode);
    rebuild_category_rows();

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    EditDialogResult result;
    result.name = name_edit->text().trimmed();
    if (result.name.isEmpty()) {
        return std::nullopt;
    }
    result.categories = split_lines(cats_edit->toPlainText());
    result.subcategories_by_category = collect_category_row_values(row_edits);
    result.subcategories = result.subcategories_by_category.empty()
        ? split_lines(subs_edit->toPlainText())
        : std::vector<std::string>{};
    return result;
}
}

WhitelistManagerDialog::WhitelistManagerDialog(WhitelistStore& store, QWidget* parent)
    : QDialog(parent), store_(store)
{
    setWindowTitle(tr("Category whitelists"));
    auto* layout = new QVBoxLayout(this);

    list_widget_ = new QListWidget(this);
    layout->addWidget(list_widget_);

    auto* button_row = new QHBoxLayout();
    add_button_ = new QPushButton(tr("Add"), this);
    edit_button_ = new QPushButton(tr("Edit"), this);
    remove_button_ = new QPushButton(tr("Remove"), this);
    button_row->addWidget(add_button_);
    button_row->addWidget(edit_button_);
    button_row->addWidget(remove_button_);
    button_row->addStretch();
    layout->addLayout(button_row);

    auto* close_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(close_box);

    connect(add_button_, &QPushButton::clicked, this, [this]() { on_add_clicked(); });
    connect(edit_button_, &QPushButton::clicked, this, [this]() { on_edit_clicked(); });
    connect(remove_button_, &QPushButton::clicked, this, [this]() { on_remove_clicked(); });
    connect(list_widget_, &QListWidget::currentRowChanged, this, [this](int row) { on_selection_changed(row); });
    connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refresh_list();
}

void WhitelistManagerDialog::refresh_list()
{
    if (!list_widget_) return;
    if (store_.empty()) {
        store_.ensure_default_from_legacy({}, {});
        store_.save();
    }
    list_widget_->clear();
    for (const auto& name : store_.list_names()) {
        auto* item = new QListWidgetItem(QString::fromStdString(name));
        item->setData(Qt::UserRole, QString::fromStdString(name));
        list_widget_->addItem(item);
    }
    on_selection_changed(list_widget_->currentRow());
}

bool WhitelistManagerDialog::edit_entry(const QString& name, WhitelistEntry& entry)
{
    auto result = show_edit_dialog(this, name, entry);
    if (!result.has_value()) {
        return false;
    }

    store_.remove(name.toStdString());
    store_.set(result->name.toStdString(),
               WhitelistEntry{
                   result->categories,
                   result->subcategories,
                   result->subcategories_by_category
               });
    store_.save();
    refresh_list();
    notify_changed();
    return true;
}

void WhitelistManagerDialog::on_add_clicked()
{
    WhitelistEntry entry;
    edit_entry(QString(), entry);
}

void WhitelistManagerDialog::on_edit_clicked()
{
    if (!list_widget_) return;
    auto* item = list_widget_->currentItem();
    if (!item) return;
    const QString name = item->text();
    if (auto existing = store_.get(name.toStdString())) {
        edit_entry(name, *existing);
    }
}

void WhitelistManagerDialog::on_remove_clicked()
{
    if (!list_widget_) return;
    auto* item = list_widget_->currentItem();
    if (!item) return;
    const QString name = item->text();
    if (name == QString::fromStdString(store_.default_name())) {
        QMessageBox::warning(this, tr("Cannot remove"), tr("The default list cannot be removed."));
        return;
    }
    store_.remove(name.toStdString());
    store_.save();
    refresh_list();
    notify_changed();
}

void WhitelistManagerDialog::on_selection_changed(int row)
{
    const bool has_selection = row >= 0;
    edit_button_->setEnabled(has_selection);
    remove_button_->setEnabled(has_selection);
}

void WhitelistManagerDialog::notify_changed()
{
    if (on_lists_changed_) {
        on_lists_changed_();
    }
}
