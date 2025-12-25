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
#include <QCheckBox>
#include <QAbstractItemView>

namespace {
QString join_lines(const std::vector<std::string>& items) {
    QStringList list;
    for (const auto& i : items) {
        list << QString::fromStdString(i);
    }
    // Use semicolon separator
    return list.join("; ");
}

std::vector<std::string> split_lines(const QString& text) {
    std::vector<std::string> items;
    // Try semicolon first (new format)
    if (text.contains(';')) {
        for (const auto& part : text.split(";")) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) {
                items.emplace_back(trimmed.toStdString());
            }
        }
    } else {
        // Fall back to comma for backward compatibility
        for (const auto& part : text.split(",")) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) {
                items.emplace_back(trimmed.toStdString());
            }
        }
    }
    return items;
}

struct EditDialogResult {
    QString name;
    std::vector<std::string> categories;
    std::vector<std::string> subcategories;
    std::string context;
    bool enable_advanced_subcategories;
};

std::optional<EditDialogResult> show_edit_dialog(QWidget* parent,
                                                 const QString& initial_name,
                                                 const WhitelistEntry& entry)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Edit whitelist"));
    dialog.resize(600, 500);
    auto* layout = new QVBoxLayout(&dialog);

    auto* name_edit = new QLineEdit(&dialog);
    name_edit->setText(initial_name);
    layout->addWidget(new QLabel(QObject::tr("Name:"), &dialog));
    layout->addWidget(name_edit);

    auto* cats_edit = new QTextEdit(&dialog);
    cats_edit->setPlainText(join_lines(entry.categories));
    layout->addWidget(new QLabel(QObject::tr("Categories (semicolon separated):"), &dialog));
    layout->addWidget(cats_edit);

    auto* subs_edit = new QTextEdit(&dialog);
    subs_edit->setPlainText(join_lines(entry.subcategories));
    layout->addWidget(new QLabel(QObject::tr("Subcategories (semicolon separated):"), &dialog));
    layout->addWidget(subs_edit);

    auto* context_edit = new QTextEdit(&dialog);
    context_edit->setPlainText(QString::fromStdString(entry.context));
    context_edit->setMaximumHeight(80);
    layout->addWidget(new QLabel(QObject::tr("Context (describe what files/folders are being sorted):"), &dialog));
    layout->addWidget(context_edit);

    auto* advanced_checkbox = new QCheckBox(QObject::tr("Enable advanced subcategory generation"), &dialog);
    advanced_checkbox->setChecked(entry.enable_advanced_subcategories);
    advanced_checkbox->setToolTip(QObject::tr("Generate dynamic subcategories based on categories and files being processed"));
    layout->addWidget(advanced_checkbox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    EditDialogResult result;
    result.name = name_edit->text().trimmed();
    if (result.name.isEmpty()) {
        return std::nullopt;
    }
    result.categories = split_lines(cats_edit->toPlainText());
    result.subcategories = split_lines(subs_edit->toPlainText());
    result.context = context_edit->toPlainText().toStdString();
    result.enable_advanced_subcategories = advanced_checkbox->isChecked();
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
    WhitelistEntry new_entry;
    new_entry.categories = result->categories;
    new_entry.subcategories = result->subcategories;
    new_entry.context = result->context;
    new_entry.enable_advanced_subcategories = result->enable_advanced_subcategories;
    store_.set(result->name.toStdString(), new_entry);
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
