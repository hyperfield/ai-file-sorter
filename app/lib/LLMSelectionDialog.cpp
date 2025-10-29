#include "LLMSelectionDialog.hpp"

#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
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
{
    setWindowTitle(tr("Choose LLM Mode"));
    setModal(true);
    setup_ui();
    connect_signals();

    selected_choice = settings.get_llm_choice();
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
    default:
        remote_radio->setChecked(true);
        selected_choice = LLMChoice::Remote;
        break;
    }

    update_ui_for_choice();
    resize(520, sizeHint().height());
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

    radio_layout->addWidget(local7_radio);
    radio_layout->addWidget(local7_desc);
    radio_layout->addWidget(local3_radio);
    radio_layout->addWidget(local3_desc);
    radio_layout->addWidget(remote_radio);
    radio_layout->addWidget(remote_desc);

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

    connect(download_button, &QPushButton::clicked, this, &LLMSelectionDialog::start_download);
    connect(button_box, &QDialogButtonBox::accepted, this, &LLMSelectionDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &LLMSelectionDialog::reject);
}


LLMChoice LLMSelectionDialog::get_selected_llm_choice() const
{
    return selected_choice;
}


void LLMSelectionDialog::set_status_message(const QString& message)
{
    status_label->setText(message);
}


void LLMSelectionDialog::update_ui_for_choice()
{
    if (remote_radio->isChecked()) {
        selected_choice = LLMChoice::Remote;
    } else if (local3_radio->isChecked()) {
        selected_choice = LLMChoice::Local_3b;
    } else if (local7_radio->isChecked()) {
        selected_choice = LLMChoice::Local_7b;
    }

    const bool is_local = (selected_choice == LLMChoice::Local_3b || selected_choice == LLMChoice::Local_7b);
    download_section->setVisible(is_local);

    if (!is_local) {
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
