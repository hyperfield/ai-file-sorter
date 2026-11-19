#include "CustomApiDialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

CustomApiDialog::CustomApiDialog(QWidget* parent)
    : QDialog(parent)
{
    setup_ui();
    wire_signals();
}

CustomApiDialog::CustomApiDialog(QWidget* parent, const CustomApiEndpoint& existing)
    : QDialog(parent)
{
    setup_ui();
    wire_signals();
    apply_existing(existing);
}

void CustomApiDialog::setup_ui()
{
    setWindowTitle(tr("Custom OpenAI-compatible API"));
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    name_edit = new QLineEdit(this);
    description_edit = new QTextEdit(this);
    description_edit->setFixedHeight(70);

    base_url_edit = new QLineEdit(this);
    base_url_edit->setPlaceholderText(tr("e.g. http://localhost:1234/v1"));

    model_edit = new QLineEdit(this);
    model_edit->setPlaceholderText(tr("e.g. llama-3.1, gpt-4o-mini"));

    api_key_edit = new QLineEdit(this);
    api_key_edit->setEchoMode(QLineEdit::Password);
    api_key_edit->setClearButtonEnabled(true);
    show_api_key_checkbox = new QCheckBox(tr("Show"), this);
    auto* api_key_row = new QWidget(this);
    auto* api_key_layout = new QHBoxLayout(api_key_row);
    api_key_layout->setContentsMargins(0, 0, 0, 0);
    api_key_layout->addWidget(api_key_edit, 1);
    api_key_layout->addWidget(show_api_key_checkbox);

    form->addRow(tr("Display name"), name_edit);
    form->addRow(tr("Description"), description_edit);
    form->addRow(tr("Base URL or endpoint"), base_url_edit);
    form->addRow(tr("Model"), model_edit);
    form->addRow(tr("API key (optional)"), api_key_row);
    image_analysis_checkbox = new QCheckBox(tr("Vision-capable model"), this);
    image_analysis_checkbox->setObjectName(QStringLiteral("apiVisionCapability"));
    image_analysis_checkbox->setToolTip(tr("Makes this connection available in the visual model selector. Image contents are sent to the selected server."));
    form->addRow(QString(), image_analysis_checkbox);

    layout->addLayout(form);

    auto* hint = new QLabel(tr("Enter a base URL (e.g. http://localhost:1234/v1) or a full /chat/completions endpoint."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button = button_box->button(QDialogButtonBox::Ok);
    ok_button->setEnabled(false);
    layout->addWidget(button_box);

    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CustomApiDialog::wire_signals()
{
    connect(name_edit, &QLineEdit::textChanged, this, &CustomApiDialog::validate_inputs);
    connect(base_url_edit, &QLineEdit::textChanged, this, &CustomApiDialog::validate_inputs);
    connect(model_edit, &QLineEdit::textChanged, this, &CustomApiDialog::validate_inputs);
    connect(image_analysis_checkbox, &QCheckBox::clicked, this, [this](bool checked) {
        if (checked && !confirm_image_upload()) {
            image_analysis_checkbox->setChecked(false);
        }
    });
    if (show_api_key_checkbox) {
        connect(show_api_key_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (api_key_edit) {
                api_key_edit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
            }
        });
    }
}

bool CustomApiDialog::confirm_image_upload()
{
    QMessageBox box(this);
    box.setObjectName(QStringLiteral("imageUploadConfirmation"));
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Send images to server?"));
    box.setText(tr("When this connection is selected for image analysis, the scanned images will be sent to the configured API server for processing.\n\nClick OK to enable this setting or Cancel to leave it disabled."));
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.setEscapeButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Ok;
}

void CustomApiDialog::apply_existing(const CustomApiEndpoint& existing)
{
    name_edit->setText(QString::fromStdString(existing.name));
    description_edit->setPlainText(QString::fromStdString(existing.description));
    base_url_edit->setText(QString::fromStdString(existing.base_url));
    model_edit->setText(QString::fromStdString(existing.model));
    api_key_edit->setText(QString::fromStdString(existing.api_key));
    image_analysis_checkbox->setChecked(existing.image_analysis_enabled);
    validate_inputs();
}

void CustomApiDialog::validate_inputs()
{
    const bool valid = !name_edit->text().trimmed().isEmpty()
        && !base_url_edit->text().trimmed().isEmpty()
        && !model_edit->text().trimmed().isEmpty();
    if (ok_button) {
        ok_button->setEnabled(valid);
    }
}

CustomApiEndpoint CustomApiDialog::result() const
{
    CustomApiEndpoint endpoint;
    endpoint.name = name_edit->text().trimmed().toStdString();
    endpoint.description = description_edit->toPlainText().trimmed().toStdString();
    endpoint.base_url = base_url_edit->text().trimmed().toStdString();
    endpoint.model = model_edit->text().trimmed().toStdString();
    endpoint.api_key = api_key_edit->text().trimmed().toStdString();
    endpoint.image_analysis_enabled = image_analysis_checkbox->isChecked();
    return endpoint;
}
