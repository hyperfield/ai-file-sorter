#include "BulkEditDialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>

BulkEditDialog::BulkEditDialog(bool allow_subcategory, QWidget* parent)
    : QDialog(parent),
      allow_subcategory_(allow_subcategory)
{
    setWindowTitle(QObject::tr("Edit selected items"));

    auto* layout = new QVBoxLayout(this);
    auto* form_layout = new QFormLayout();

    category_edit_ = new QLineEdit(this);
    category_edit_->setPlaceholderText(QObject::tr("Leave empty to keep existing"));
    form_layout->addRow(QObject::tr("Category"), category_edit_);

    if (allow_subcategory_) {
        subcategory_edit_ = new QLineEdit(this);
        subcategory_edit_->setPlaceholderText(QObject::tr("Leave empty to keep existing"));
        form_layout->addRow(QObject::tr("Subcategory"), subcategory_edit_);
    }

    layout->addLayout(form_layout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button_ = buttons->button(QDialogButtonBox::Ok);
    if (ok_button_) {
        ok_button_->setEnabled(false);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(category_edit_, &QLineEdit::textChanged, this, &BulkEditDialog::update_ok_state);
    if (subcategory_edit_) {
        connect(subcategory_edit_, &QLineEdit::textChanged, this, &BulkEditDialog::update_ok_state);
    }

    layout->addWidget(buttons);
    update_ok_state();
}

std::string BulkEditDialog::category() const
{
    return category_edit_ ? category_edit_->text().trimmed().toStdString() : std::string();
}

std::string BulkEditDialog::subcategory() const
{
    if (!allow_subcategory_ || !subcategory_edit_) {
        return std::string();
    }
    return subcategory_edit_->text().trimmed().toStdString();
}

void BulkEditDialog::update_ok_state()
{
    const bool has_category = category_edit_ && !category_edit_->text().trimmed().isEmpty();
    const bool has_subcategory = allow_subcategory_ && subcategory_edit_ &&
                                 !subcategory_edit_->text().trimmed().isEmpty();
    if (ok_button_) {
        ok_button_->setEnabled(has_category || has_subcategory);
    }
}
