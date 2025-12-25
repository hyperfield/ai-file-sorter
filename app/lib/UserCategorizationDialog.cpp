#include "UserCategorizationDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QString>

UserCategorizationDialog::UserCategorizationDialog(const std::string& file_name,
                                                 const std::string& error_reason,
                                                 QWidget* parent)
    : QDialog(parent), skip_file(false)
{
    setWindowTitle(tr("Manual Categorization Required"));
    setMinimumWidth(500);

    auto* main_layout = new QVBoxLayout(this);

    // Info section
    auto* info_label = new QLabel(tr("The AI was unable to automatically categorize this file."));
    info_label->setWordWrap(true);
    main_layout->addWidget(info_label);

    // File name
    file_label = new QLabel(QString::fromStdString(file_name));
    file_label->setStyleSheet("font-weight: bold; padding: 10px; background-color: #f0f0f0;");
    file_label->setWordWrap(true);
    main_layout->addWidget(file_label);

    // Reason
    reason_label = new QLabel(QString::fromStdString("Reason: " + error_reason));
    reason_label->setWordWrap(true);
    reason_label->setStyleSheet("color: #666; font-style: italic;");
    main_layout->addWidget(reason_label);

    main_layout->addSpacing(20);

    // Input form
    auto* form_layout = new QFormLayout();
    
    category_input = new QLineEdit();
    category_input->setPlaceholderText(tr("e.g., Documents, Images, Videos"));
    form_layout->addRow(tr("Category:"), category_input);

    subcategory_input = new QLineEdit();
    subcategory_input->setPlaceholderText(tr("e.g., Work, Personal, Archive"));
    form_layout->addRow(tr("Subcategory:"), subcategory_input);

    main_layout->addLayout(form_layout);

    main_layout->addSpacing(10);

    // Help text
    auto* help_label = new QLabel(tr(
        "Please provide a category and subcategory for this file. "
        "Your choice will be remembered to help with similar files in the future."));
    help_label->setWordWrap(true);
    help_label->setStyleSheet("font-size: 10pt; color: #888;");
    main_layout->addWidget(help_label);

    main_layout->addSpacing(20);

    // Buttons
    auto* button_box = new QDialogButtonBox();
    
    auto* ok_button = button_box->addButton(QDialogButtonBox::Ok);
    ok_button->setText(tr("Categorize"));
    
    auto* skip_button = button_box->addButton(tr("Skip This File"), QDialogButtonBox::ActionRole);
    auto* cancel_button = button_box->addButton(QDialogButtonBox::Cancel);
    cancel_button->setText(tr("Cancel All"));

    main_layout->addWidget(button_box);

    // Connect signals
    connect(ok_button, &QPushButton::clicked, this, [this]() {
        if (!category_input->text().trimmed().isEmpty() && 
            !subcategory_input->text().trimmed().isEmpty()) {
            skip_file = false;
            accept();
        }
    });

    connect(skip_button, &QPushButton::clicked, this, [this]() {
        skip_file = true;
        accept();
    });

    connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);

    // Focus on category input
    category_input->setFocus();
}

std::string UserCategorizationDialog::get_category() const
{
    return category_input->text().trimmed().toStdString();
}

std::string UserCategorizationDialog::get_subcategory() const
{
    return subcategory_input->text().trimmed().toStdString();
}

bool UserCategorizationDialog::should_skip() const
{
    return skip_file;
}
