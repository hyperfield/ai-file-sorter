#include "CustomLLMDialog.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>


CustomLLMDialog::CustomLLMDialog(QWidget* parent)
    : QDialog(parent)
{
    setup_ui();
    wire_signals();
}

CustomLLMDialog::CustomLLMDialog(QWidget* parent, const CustomLLM& existing)
    : QDialog(parent)
{
    setup_ui();
    wire_signals();
    apply_existing(existing);
}

void CustomLLMDialog::setup_ui()
{
    setWindowTitle(tr("Custom local LLM"));
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    name_edit = new QLineEdit(this);
    description_edit = new QTextEdit(this);
    description_edit->setFixedHeight(70);
    path_edit = new QLineEdit(this);
    browse_button = new QPushButton(tr("Browse…"), this);
    auto* path_row = new QHBoxLayout();
    path_row->addWidget(path_edit, 1);
    path_row->addWidget(browse_button);

    form->addRow(tr("Display name"), name_edit);
    form->addRow(tr("Description"), description_edit);
    form->addRow(tr("Model file (.gguf)"), path_row);

    layout->addLayout(form);

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button = button_box->button(QDialogButtonBox::Ok);
    ok_button->setEnabled(false);
    layout->addWidget(button_box);

    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CustomLLMDialog::wire_signals()
{
    connect(name_edit, &QLineEdit::textChanged, this, &CustomLLMDialog::validate_inputs);
    connect(path_edit, &QLineEdit::textChanged, this, &CustomLLMDialog::validate_inputs);
    connect(browse_button, &QPushButton::clicked, this, &CustomLLMDialog::browse_for_file);
}

void CustomLLMDialog::apply_existing(const CustomLLM& existing)
{
    name_edit->setText(QString::fromStdString(existing.name));
    description_edit->setPlainText(QString::fromStdString(existing.description));
    path_edit->setText(QString::fromStdString(existing.path));
    validate_inputs();
}

void CustomLLMDialog::validate_inputs()
{
    const bool valid = !name_edit->text().trimmed().isEmpty() &&
                       !path_edit->text().trimmed().isEmpty();
    if (ok_button) {
        ok_button->setEnabled(valid);
    }
}

void CustomLLMDialog::browse_for_file()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Select .gguf model"),
                                                      QString(),
                                                      tr("GGUF models (*.gguf);;All files (*.*)"));
    if (!path.isEmpty()) {
        path_edit->setText(path);
    }
}

CustomLLM CustomLLMDialog::result() const
{
    CustomLLM llm;
    llm.name = name_edit->text().trimmed().toStdString();
    llm.description = description_edit->toPlainText().trimmed().toStdString();
    llm.path = path_edit->text().trimmed().toStdString();
    return llm;
}
