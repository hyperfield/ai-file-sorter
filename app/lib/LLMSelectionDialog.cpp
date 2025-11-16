#include "LLMSelectionDialog.hpp"

#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "CustomLLMDialog.hpp"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QString>

#include <cstdlib>


namespace {

QString format_markup_label(const QString& title, const QString& value, const QString& color)
{
    return QStringLiteral("<b>%1:</b> <span style=\"color:%2\">%3</span>")
        .arg(title, color, value.toHtmlEscaped());
}

} // namespace


LLMSelectionDialog::LLMSelectionDialog(Settings& settings, QWidget* parent)
    : QDialog(parent)
    , settings(settings)
{
    setWindowTitle(tr("Choose LLM Mode"));
    setModal(true);
    setup_ui();
    connect_signals();

    selected_choice = settings.get_llm_choice();
    selected_custom_id = settings.get_active_custom_llm_id();
    switch (selected_choice) {
    case LLMChoice::Remote:
        remote_radio->setChecked(true);
        break;
    case LLMChoice::Local_3b:
        local3_radio->setChecked(true);
        break;
    case LLMChoice::Local_7b:
        local7_radio->setChecked(true);
        break;
    case LLMChoice::Custom:
        custom_radio->setChecked(true);
        break;
    default:
        remote_radio->setChecked(true);
        selected_choice = LLMChoice::Remote;
        break;
    }
    refresh_custom_lists();
    if (selected_choice == LLMChoice::Custom) {
        select_custom_by_id(selected_custom_id);
    }

    update_ui_for_choice();
    resize(620, sizeHint().height());
}


LLMSelectionDialog::~LLMSelectionDialog()
{
    if (downloader && is_downloading.load()) {
        downloader->cancel_download();
    }
}


void LLMSelectionDialog::setup_ui()
{
    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(tr("Select LLM Mode:"), this);
    title->setAlignment(Qt::AlignHCenter);
    layout->addWidget(title);

    auto* radio_container = new QWidget(this);
    auto* radio_layout = new QVBoxLayout(radio_container);
    radio_layout->setSpacing(10);

    local7_radio = new QRadioButton(tr("Local LLM (Mistral 7b Instruct v0.2 Q5)"), radio_container);
    auto* local7_desc = new QLabel(tr("Quite precise. Slower on CPU, but performs much better with GPU acceleration.\nSupports: Nvidia (CUDA), Apple (Metal), CPU."), radio_container);
    local7_desc->setWordWrap(true);

    local3_radio = new QRadioButton(tr("Local LLM (LLaMa 3b v3.2 Instruct Q8)"), radio_container);
    auto* local3_desc = new QLabel(tr("Less precise, but works quickly even on CPUs. Good for lightweight local use."), radio_container);
    local3_desc->setWordWrap(true);

    remote_radio = new QRadioButton(tr("Remote LLM (ChatGPT 4o-mini)"), radio_container);
    auto* remote_desc = new QLabel(tr("Fast and accurate, but requires internet connection."), radio_container);
    remote_desc->setWordWrap(true);

    custom_radio = new QRadioButton(tr("Custom local LLM (gguf)"), radio_container);
    auto* custom_row = new QWidget(radio_container);
    auto* custom_layout = new QHBoxLayout(custom_row);
    custom_layout->setContentsMargins(24, 0, 0, 0);
    custom_combo = new QComboBox(custom_row);
    custom_combo->setMinimumContentsLength(18);
    custom_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    add_custom_button = new QPushButton(tr("Add…"), custom_row);
    edit_custom_button = new QPushButton(tr("Edit…"), custom_row);
    delete_custom_button = new QPushButton(tr("Delete"), custom_row);
    custom_layout->addWidget(custom_combo, 1);
    custom_layout->addWidget(add_custom_button);
    custom_layout->addWidget(edit_custom_button);
    custom_layout->addWidget(delete_custom_button);

    radio_layout->addWidget(local7_radio);
    radio_layout->addWidget(local7_desc);
    radio_layout->addWidget(local3_radio);
    radio_layout->addWidget(local3_desc);
    radio_layout->addWidget(remote_radio);
    radio_layout->addWidget(remote_desc);
    radio_layout->addWidget(custom_radio);
    radio_layout->addWidget(custom_row);

    layout->addWidget(radio_container);

    download_section = new QWidget(this);
    auto* download_layout = new QVBoxLayout(download_section);
    download_layout->setSpacing(6);

    remote_url_label = new QLabel(download_section);
    remote_url_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    local_path_label = new QLabel(download_section);
    local_path_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    file_size_label = new QLabel(download_section);
    file_size_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    status_label = new QLabel(download_section);
    status_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    progress_bar = new QProgressBar(download_section);
    progress_bar->setRange(0, 100);
    progress_bar->setValue(0);
    progress_bar->setVisible(false);

    download_button = new QPushButton(tr("Download"), download_section);
    download_button->setEnabled(false);

    download_layout->addWidget(remote_url_label);
    download_layout->addWidget(local_path_label);
    download_layout->addWidget(file_size_label);
    download_layout->addWidget(status_label);
    download_layout->addWidget(progress_bar);
    download_layout->addWidget(download_button, 0, Qt::AlignLeft);

    layout->addWidget(download_section);
    download_section->setVisible(false);

    button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(button_box);
}


void LLMSelectionDialog::connect_signals()
{
    auto update_handler = [this]() { update_ui_for_choice(); };
    connect(remote_radio, &QRadioButton::toggled, this, update_handler);
    connect(local3_radio, &QRadioButton::toggled, this, update_handler);
    connect(local7_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_combo, &QComboBox::currentTextChanged, this, update_handler);
    connect(add_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_add_custom);
    connect(edit_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_edit_custom);
    connect(delete_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_delete_custom);

    connect(download_button, &QPushButton::clicked, this, &LLMSelectionDialog::start_download);
    connect(button_box, &QDialogButtonBox::accepted, this, &LLMSelectionDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &LLMSelectionDialog::reject);
}


LLMChoice LLMSelectionDialog::get_selected_llm_choice() const
{
    return selected_choice;
}

std::string LLMSelectionDialog::get_selected_custom_llm_id() const
{
    return selected_custom_id;
}


void LLMSelectionDialog::set_status_message(const QString& message)
{
    status_label->setText(message);
}


void LLMSelectionDialog::update_ui_for_choice()
{
    update_custom_buttons();

    if (remote_radio->isChecked()) {
        selected_choice = LLMChoice::Remote;
    } else if (local3_radio->isChecked()) {
        selected_choice = LLMChoice::Local_3b;
    } else if (local7_radio->isChecked()) {
        selected_choice = LLMChoice::Local_7b;
    } else if (custom_radio->isChecked()) {
        selected_choice = LLMChoice::Custom;
    }

    const bool is_local_builtin = (selected_choice == LLMChoice::Local_3b || selected_choice == LLMChoice::Local_7b);
    const bool is_custom = selected_choice == LLMChoice::Custom;
    download_section->setVisible(is_local_builtin);

    custom_combo->setEnabled(is_custom);
    edit_custom_button->setEnabled(is_custom && custom_combo->currentIndex() >= 0 && custom_combo->count() > 0);
    delete_custom_button->setEnabled(is_custom && custom_combo->currentIndex() >= 0 && custom_combo->count() > 0);

    if (is_custom) {
        if (custom_combo->currentIndex() >= 0) {
            selected_custom_id = custom_combo->currentData().toString().toStdString();
        } else {
            selected_custom_id.clear();
        }
        button_box->button(QDialogButtonBox::Ok)->setEnabled(!selected_custom_id.empty());
        progress_bar->setVisible(false);
        download_button->setVisible(false);
        set_status_message(selected_custom_id.empty() ? tr("Choose or add a custom model.") : tr("Custom model selected."));
        return;
    }

    if (!is_local_builtin) {
        button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
        return;
    }

    refresh_downloader();

    if (!downloader) {
        button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
        download_button->setEnabled(false);
        return;
    }

    update_download_info();

    const auto status = downloader->get_download_status();
    switch (status) {
    case LLMDownloader::DownloadStatus::Complete:
        progress_bar->setVisible(true);
        progress_bar->setValue(100);
        download_button->setEnabled(false);
        download_button->setVisible(false);
        button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
        set_status_message(tr("Model ready."));
        break;
    case LLMDownloader::DownloadStatus::InProgress:
        progress_bar->setVisible(true);
        download_button->setVisible(true);
        download_button->setEnabled(!is_downloading.load());
        download_button->setText(tr("Resume download"));
        button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
        set_status_message(tr("Partial download detected. You can resume."));
        break;
    case LLMDownloader::DownloadStatus::NotStarted:
    default:
        progress_bar->setVisible(false);
        progress_bar->setValue(0);
        download_button->setVisible(true);
        download_button->setEnabled(!is_downloading.load());
        download_button->setText(tr("Download"));
        button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
        set_status_message(tr("Download required."));
        break;
    }
}


void LLMSelectionDialog::refresh_downloader()
{
    const std::string env_var = current_download_env_var();
    if (env_var.empty()) {
        downloader.reset();
        set_status_message(tr("Unsupported LLM selection."));
        return;
    }

    const char* env_url = std::getenv(env_var.c_str());
    if (!env_url) {
        downloader.reset();
        set_status_message(tr("Missing download URL environment variable (%1)." ).arg(QString::fromStdString(env_var)));
        return;
    }

    if (!downloader) {
        downloader = std::make_unique<LLMDownloader>(env_url);
    } else {
        downloader->set_download_url(env_url);
    }

    try {
        downloader->init_if_needed();
    } catch (const std::exception& ex) {
        set_status_message(QString::fromStdString(ex.what()));
        downloader.reset();
    }
}


void LLMSelectionDialog::update_download_info()
{
    if (!downloader) {
        return;
    }

    remote_url_label->setText(format_markup_label(tr("Remote URL"),
                                                  QString::fromStdString(downloader->get_download_url()),
                                                  QStringLiteral("#1565c0")));

    local_path_label->setText(format_markup_label(tr("Local path"),
                                                 QString::fromStdString(downloader->get_download_destination()),
                                                 QStringLiteral("#2e7d32")));

    const auto size = downloader->get_real_content_length();
    if (size > 0) {
        file_size_label->setText(format_markup_label(tr("File size"),
                                                     QString::fromStdString(Utils::format_size(size)),
                                                     QStringLiteral("#333")));
    } else {
        file_size_label->setText(tr("File size: unknown"));
    }
}

void LLMSelectionDialog::refresh_custom_lists()
{
    if (!custom_combo) {
        return;
    }

    custom_combo->blockSignals(true);
    custom_combo->clear();
    for (const auto& entry : settings.get_custom_llms()) {
        custom_combo->addItem(QString::fromStdString(entry.name),
                              QString::fromStdString(entry.id));
    }
    if (!selected_custom_id.empty()) {
        select_custom_by_id(selected_custom_id);
    } else if (custom_combo->count() > 0) {
        custom_combo->setCurrentIndex(0);
        selected_custom_id = custom_combo->currentData().toString().toStdString();
    }
    custom_combo->blockSignals(false);
    update_custom_buttons();
}

void LLMSelectionDialog::select_custom_by_id(const std::string& id)
{
    for (int i = 0; i < custom_combo->count(); ++i) {
        if (custom_combo->itemData(i).toString().toStdString() == id) {
            custom_combo->setCurrentIndex(i);
            return;
        }
    }
    if (custom_combo->count() > 0) {
        custom_combo->setCurrentIndex(0);
    }
}

void LLMSelectionDialog::handle_add_custom()
{
    CustomLLMDialog editor(this);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    CustomLLM entry = editor.result();
    selected_custom_id = settings.upsert_custom_llm(entry);
    refresh_custom_lists();
    select_custom_by_id(selected_custom_id);
    custom_radio->setChecked(true);
    update_ui_for_choice();
}

void LLMSelectionDialog::handle_edit_custom()
{
    if (!custom_combo || custom_combo->currentIndex() < 0) {
        return;
    }
    const std::string id = custom_combo->currentData().toString().toStdString();
    CustomLLM entry = settings.find_custom_llm(id);
    if (entry.id.empty()) {
        return;
    }

    CustomLLMDialog editor(this, entry);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    CustomLLM updated = editor.result();
    updated.id = entry.id;
    selected_custom_id = settings.upsert_custom_llm(updated);
    refresh_custom_lists();
    select_custom_by_id(selected_custom_id);
    custom_radio->setChecked(true);
    update_ui_for_choice();
}

void LLMSelectionDialog::handle_delete_custom()
{
    if (!custom_combo || custom_combo->currentIndex() < 0) {
        return;
    }
    const std::string id = custom_combo->currentData().toString().toStdString();
    const QString name = custom_combo->currentText();
    const auto response = QMessageBox::question(this,
                                                tr("Delete custom model"),
                                                tr("Remove '%1' from your custom LLMs? This does not delete the file on disk.")
                                                    .arg(name));
    if (response != QMessageBox::Yes) {
        return;
    }
    settings.remove_custom_llm(id);
    if (selected_custom_id == id) {
        selected_custom_id.clear();
    }
    refresh_custom_lists();
    custom_radio->setChecked(custom_combo->count() > 0);
    update_ui_for_choice();
}

void LLMSelectionDialog::update_custom_buttons()
{
    const bool has_selection = custom_combo && custom_combo->currentIndex() >= 0 && custom_combo->count() > 0;
    if (edit_custom_button) {
        edit_custom_button->setEnabled(has_selection && custom_radio->isChecked());
    }
    if (delete_custom_button) {
        delete_custom_button->setEnabled(has_selection && custom_radio->isChecked());
    }
}


void LLMSelectionDialog::start_download()
{
    if (!downloader || is_downloading.load()) {
        return;
    }

    if (!Utils::is_network_available()) {
        DialogUtils::show_error_dialog(this, ERR_NO_INTERNET_CONNECTION);
        return;
    }

    try {
        downloader->init_if_needed();
    } catch (const std::exception& ex) {
        DialogUtils::show_error_dialog(this, ex.what());
        return;
    }

    is_downloading = true;
    download_button->setEnabled(false);
    progress_bar->setVisible(true);
    set_status_message(tr("Downloading…"));
    progress_bar->setValue(0);
    button_box->button(QDialogButtonBox::Ok)->setEnabled(false);

    downloader->start_download(
        [this](double fraction) {
            QMetaObject::invokeMethod(this, [this, fraction]() {
                progress_bar->setVisible(true);
                progress_bar->setValue(static_cast<int>(fraction * 100));
            }, Qt::QueuedConnection);
        },
        [this]() {
            QMetaObject::invokeMethod(this, [this]() {
                is_downloading = false;
                set_status_message(tr("Download complete."));
                update_ui_for_choice();
            }, Qt::QueuedConnection);
        },
        [this](const std::string& text) {
            QMetaObject::invokeMethod(this, [this, text]() {
                set_status_message(QString::fromStdString(text));
            }, Qt::QueuedConnection);
        },
        [this](const std::string& error_text) {
            QMetaObject::invokeMethod(this, [this, error_text]() {
                is_downloading = false;
                progress_bar->setVisible(false);
                download_button->setEnabled(true);

                const QString error = QString::fromStdString(error_text);
                if (error.compare(QStringLiteral("Download cancelled"), Qt::CaseInsensitive) == 0) {
                    set_status_message(tr("Download cancelled."));
                } else {
                    set_status_message(tr("Download error: %1").arg(error));
                }
                button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
            }, Qt::QueuedConnection);
        });
}


std::string LLMSelectionDialog::current_download_env_var() const
{
    if (selected_choice == LLMChoice::Local_3b) {
        return "LOCAL_LLM_3B_DOWNLOAD_URL";
    }
    if (selected_choice == LLMChoice::Local_7b) {
        return "LOCAL_LLM_7B_DOWNLOAD_URL";
    }
    return std::string();
}
