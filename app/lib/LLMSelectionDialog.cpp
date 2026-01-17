#include "LLMSelectionDialog.hpp"

#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "CustomLLMDialog.hpp"
#ifdef AI_FILE_SORTER_TEST_BUILD
#include "LLMSelectionDialogTestAccess.hpp"
#endif

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QFileDialog>
#include <QLineEdit>
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

    openai_api_key = settings.get_openai_api_key();
    openai_model = settings.get_openai_model();
    gemini_api_key = settings.get_gemini_api_key();
    gemini_model = settings.get_gemini_model();
    if (openai_api_key_edit) {
        openai_api_key_edit->setText(QString::fromStdString(openai_api_key));
    }
    if (openai_model_edit) {
        openai_model_edit->setText(QString::fromStdString(openai_model));
    }
    if (gemini_api_key_edit) {
        gemini_api_key_edit->setText(QString::fromStdString(gemini_api_key));
    }
    if (gemini_model_edit) {
        gemini_model_edit->setText(QString::fromStdString(gemini_model));
    }

    selected_choice = settings.get_llm_choice();
    selected_custom_id = settings.get_active_custom_llm_id();
    switch (selected_choice) {
    case LLMChoice::Remote_OpenAI:
        openai_radio->setChecked(true);
        break;
    case LLMChoice::Remote_Gemini:
        gemini_radio->setChecked(true);
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
        local7_radio->setChecked(true);
        selected_choice = LLMChoice::Local_7b;
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
    if (llava_model_download.downloader && llava_model_download.is_downloading.load()) {
        llava_model_download.downloader->cancel_download();
    }
    if (llava_mmproj_download.downloader && llava_mmproj_download.is_downloading.load()) {
        llava_mmproj_download.downloader->cancel_download();
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

    gemini_radio = new QRadioButton(tr("Gemini (Google AI Studio API key)"), radio_container);
    auto* gemini_desc = new QLabel(tr("Use Google's Gemini models with your AI Studio API key (internet required)."), radio_container);
    gemini_desc->setWordWrap(true);
    gemini_inputs = new QWidget(radio_container);
    auto* gemini_form = new QFormLayout(gemini_inputs);
    gemini_form->setContentsMargins(24, 0, 0, 0);
    gemini_form->setHorizontalSpacing(10);
    gemini_form->setVerticalSpacing(6);
    gemini_api_key_edit = new QLineEdit(gemini_inputs);
    gemini_api_key_edit->setEchoMode(QLineEdit::Password);
    gemini_api_key_edit->setClearButtonEnabled(true);
    gemini_api_key_edit->setPlaceholderText(tr("AIza..."));
    show_gemini_api_key_checkbox = new QCheckBox(tr("Show"), gemini_inputs);
    auto* gemini_key_row = new QWidget(gemini_inputs);
    auto* gemini_key_layout = new QHBoxLayout(gemini_key_row);
    gemini_key_layout->setContentsMargins(0, 0, 0, 0);
    gemini_key_layout->addWidget(gemini_api_key_edit, 1);
    gemini_key_layout->addWidget(show_gemini_api_key_checkbox);
    gemini_form->addRow(tr("Gemini API key"), gemini_key_row);

    gemini_model_edit = new QLineEdit(gemini_inputs);
    gemini_model_edit->setPlaceholderText(tr("e.g. gemini-2.5-flash-lite, gemini-2.5-flash, gemini-2.5-pro"));
    gemini_form->addRow(tr("Model"), gemini_model_edit);

    gemini_help_label = new QLabel(tr("Your key is stored locally in the config file for this device."), gemini_inputs);
    gemini_help_label->setWordWrap(true);
    gemini_form->addRow(gemini_help_label);
    gemini_link_label = new QLabel(gemini_inputs);
    gemini_link_label->setTextFormat(Qt::RichText);
    gemini_link_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    gemini_link_label->setOpenExternalLinks(true);
    gemini_link_label->setText(tr("<a href=\"https://aistudio.google.com/app/apikey\">Get a Gemini API key</a>"));
    gemini_form->addRow(gemini_link_label);
    gemini_inputs->setVisible(false);

    openai_radio = new QRadioButton(tr("ChatGPT (OpenAI API key)"), radio_container);
    auto* openai_desc = new QLabel(tr("Use your own OpenAI API key to access ChatGPT models (internet required)."), radio_container);
    openai_desc->setWordWrap(true);
    openai_inputs = new QWidget(radio_container);
    auto* openai_form = new QFormLayout(openai_inputs);
    openai_form->setContentsMargins(24, 0, 0, 0);
    openai_form->setHorizontalSpacing(10);
    openai_form->setVerticalSpacing(6);
    openai_api_key_edit = new QLineEdit(openai_inputs);
    openai_api_key_edit->setEchoMode(QLineEdit::Password);
    openai_api_key_edit->setClearButtonEnabled(true);
    openai_api_key_edit->setPlaceholderText(tr("sk-..."));
    show_openai_api_key_checkbox = new QCheckBox(tr("Show"), openai_inputs);
    auto* openai_key_row = new QWidget(openai_inputs);
    auto* openai_key_layout = new QHBoxLayout(openai_key_row);
    openai_key_layout->setContentsMargins(0, 0, 0, 0);
    openai_key_layout->addWidget(openai_api_key_edit, 1);
    openai_key_layout->addWidget(show_openai_api_key_checkbox);
    openai_form->addRow(tr("OpenAI API key"), openai_key_row);

    openai_model_edit = new QLineEdit(openai_inputs);
    openai_model_edit->setPlaceholderText(tr("e.g. gpt-4o-mini, gpt-4.1, o3-mini"));
    openai_form->addRow(tr("Model"), openai_model_edit);

    openai_help_label = new QLabel(tr("Your key is stored locally in the config file for this device."), openai_inputs);
    openai_help_label->setWordWrap(true);
    openai_form->addRow(openai_help_label);
    openai_link_label = new QLabel(openai_inputs);
    openai_link_label->setTextFormat(Qt::RichText);
    openai_link_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    openai_link_label->setOpenExternalLinks(true);
    openai_link_label->setText(tr("<a href=\"https://platform.openai.com/api-keys\">Get an OpenAI API key</a>"));
    openai_form->addRow(openai_link_label);
    openai_inputs->setVisible(false);

    custom_radio = new QRadioButton(tr("Custom local LLM (gguf) (experimental)"), radio_container);
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
    radio_layout->addWidget(gemini_radio);
    radio_layout->addWidget(gemini_desc);
    radio_layout->addWidget(gemini_inputs);
    radio_layout->addWidget(openai_radio);
    radio_layout->addWidget(openai_desc);
    radio_layout->addWidget(openai_inputs);
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

    visual_llm_download_section = new QGroupBox(tr("Image analysis models (LLaVA)"), this);
    auto* visual_layout = new QVBoxLayout(visual_llm_download_section);
    auto* visual_hint = new QLabel(tr("Download the visual LLM files required for image analysis."), visual_llm_download_section);
    visual_hint->setWordWrap(true);
    visual_layout->addWidget(visual_hint);

    setup_visual_llm_download_entry(llava_model_download,
                                visual_llm_download_section,
                                tr("LLaVA 1.6 Mistral 7B (text model)"),
                                "LLAVA_MODEL_URL");
    visual_layout->addWidget(llava_model_download.container);

    setup_visual_llm_download_entry(llava_mmproj_download,
                                visual_llm_download_section,
                                tr("LLaVA mmproj (vision encoder)"),
                                "LLAVA_MMPROJ_URL");
    visual_layout->addWidget(llava_mmproj_download.container);

    layout->addWidget(visual_llm_download_section);

    button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button = button_box->button(QDialogButtonBox::Ok);
    layout->addWidget(button_box);
}


void LLMSelectionDialog::connect_signals()
{
    auto update_handler = [this]() { update_ui_for_choice(); };
    connect(openai_radio, &QRadioButton::toggled, this, update_handler);
    connect(gemini_radio, &QRadioButton::toggled, this, update_handler);
    connect(local3_radio, &QRadioButton::toggled, this, update_handler);
    connect(local7_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_combo, &QComboBox::currentTextChanged, this, update_handler);
    connect(openai_api_key_edit, &QLineEdit::textChanged, this, update_handler);
    connect(openai_model_edit, &QLineEdit::textChanged, this, update_handler);
    connect(gemini_api_key_edit, &QLineEdit::textChanged, this, update_handler);
    connect(gemini_model_edit, &QLineEdit::textChanged, this, update_handler);
    connect(add_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_add_custom);
    connect(edit_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_edit_custom);
    connect(delete_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_delete_custom);

    if (show_openai_api_key_checkbox) {
        connect(show_openai_api_key_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (openai_api_key_edit) {
                openai_api_key_edit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
            }
        });
    }

    if (show_gemini_api_key_checkbox) {
        connect(show_gemini_api_key_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (gemini_api_key_edit) {
                gemini_api_key_edit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
            }
        });
    }

    connect(download_button, &QPushButton::clicked, this, &LLMSelectionDialog::start_download);
    if (llava_model_download.download_button) {
        connect(llava_model_download.download_button, &QPushButton::clicked, this, [this]() {
            start_visual_llm_download(llava_model_download);
        });
    }
    if (llava_mmproj_download.download_button) {
        connect(llava_mmproj_download.download_button, &QPushButton::clicked, this, [this]() {
            start_visual_llm_download(llava_mmproj_download);
        });
    }
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

std::string LLMSelectionDialog::get_openai_api_key() const
{
    return openai_api_key;
}

std::string LLMSelectionDialog::get_openai_model() const
{
    return openai_model;
}

std::string LLMSelectionDialog::get_gemini_api_key() const
{
    return gemini_api_key;
}

std::string LLMSelectionDialog::get_gemini_model() const
{
    return gemini_model;
}


void LLMSelectionDialog::set_status_message(const QString& message)
{
    status_label->setText(message);
}

void LLMSelectionDialog::update_ui_for_choice()
{
    update_custom_buttons();

    update_radio_selection();
    update_custom_choice_ui();
    update_visual_llm_downloads();

    const bool is_local_builtin = (selected_choice == LLMChoice::Local_3b || selected_choice == LLMChoice::Local_7b);

    if (selected_choice == LLMChoice::Custom || is_remote_choice(selected_choice) || !is_local_builtin) {
        return;
    }

    update_local_choice_ui();
}

void LLMSelectionDialog::update_radio_selection()
{
    if (openai_radio->isChecked()) {
        selected_choice = LLMChoice::Remote_OpenAI;
    } else if (gemini_radio->isChecked()) {
        selected_choice = LLMChoice::Remote_Gemini;
    } else if (local3_radio->isChecked()) {
        selected_choice = LLMChoice::Local_3b;
    } else if (local7_radio->isChecked()) {
        selected_choice = LLMChoice::Local_7b;
    } else if (custom_radio->isChecked()) {
        selected_choice = LLMChoice::Custom;
    }
}

void LLMSelectionDialog::update_custom_choice_ui()
{
    if (!ok_button && button_box) {
        ok_button = button_box->button(QDialogButtonBox::Ok);
    }
    const bool is_local_builtin = (selected_choice == LLMChoice::Local_3b || selected_choice == LLMChoice::Local_7b);
    const bool is_remote_openai = selected_choice == LLMChoice::Remote_OpenAI;
    const bool is_remote_gemini = selected_choice == LLMChoice::Remote_Gemini;
    const bool is_custom = selected_choice == LLMChoice::Custom;
    download_section->setVisible(is_local_builtin);
    if (openai_inputs) {
        openai_inputs->setVisible(is_remote_openai);
        openai_inputs->setEnabled(is_remote_openai);
    }
    if (gemini_inputs) {
        gemini_inputs->setVisible(is_remote_gemini);
        gemini_inputs->setEnabled(is_remote_gemini);
    }

    custom_combo->setEnabled(is_custom);
    edit_custom_button->setEnabled(is_custom && custom_combo->currentIndex() >= 0 && custom_combo->count() > 0);
    delete_custom_button->setEnabled(is_custom && custom_combo->currentIndex() >= 0 && custom_combo->count() > 0);

    if (is_custom) {
        if (custom_combo->currentIndex() >= 0) {
            selected_custom_id = custom_combo->currentData().toString().toStdString();
        } else {
            selected_custom_id.clear();
        }
        if (ok_button) ok_button->setEnabled(!selected_custom_id.empty());
        progress_bar->setVisible(false);
        download_button->setVisible(false);
        set_status_message(selected_custom_id.empty() ? tr("Choose or add a custom model.") : tr("Custom model selected."));
        return;
    }

    if (is_remote_openai) {
        update_openai_fields_state();
        return;
    }
    if (is_remote_gemini) {
        update_gemini_fields_state();
        return;
    }

    if (!is_local_builtin) {
        if (ok_button) ok_button->setEnabled(true);
        progress_bar->setVisible(false);
        download_button->setVisible(false);
        set_status_message(tr("Selection ready."));
        return;
    }
}

void LLMSelectionDialog::update_openai_fields_state()
{
    if (!ok_button && button_box) {
        ok_button = button_box->button(QDialogButtonBox::Ok);
    }

    const bool is_openai = selected_choice == LLMChoice::Remote_OpenAI;
    if (openai_inputs) {
        openai_inputs->setVisible(is_openai);
    }
    bool valid = false;
    if (is_openai) {
        openai_api_key = openai_api_key_edit ? openai_api_key_edit->text().trimmed().toStdString() : std::string();
        openai_model = openai_model_edit ? openai_model_edit->text().trimmed().toStdString() : std::string();
        valid = openai_inputs_valid();
        set_status_message(valid
            ? tr("ChatGPT will use your API key and model.")
            : tr("Enter your OpenAI API key and model to continue."));
    }

    if (ok_button) {
        ok_button->setEnabled(valid);
    }
    if (progress_bar) {
        progress_bar->setVisible(false);
    }
    if (download_button) {
        download_button->setVisible(false);
        download_button->setEnabled(false);
    }
}

void LLMSelectionDialog::update_gemini_fields_state()
{
    if (!ok_button && button_box) {
        ok_button = button_box->button(QDialogButtonBox::Ok);
    }

    const bool is_gemini = selected_choice == LLMChoice::Remote_Gemini;
    if (gemini_inputs) {
        gemini_inputs->setVisible(is_gemini);
    }

    bool valid = false;
    if (is_gemini) {
        gemini_api_key = gemini_api_key_edit ? gemini_api_key_edit->text().trimmed().toStdString() : std::string();
        gemini_model = gemini_model_edit ? gemini_model_edit->text().trimmed().toStdString() : std::string();
        valid = gemini_inputs_valid();
        set_status_message(valid
            ? tr("Gemini will use your API key and model.")
            : tr("Enter your Gemini API key and model to continue."));
    }

    if (ok_button) {
        ok_button->setEnabled(valid);
    }
    if (progress_bar) {
        progress_bar->setVisible(false);
    }
    if (download_button) {
        download_button->setVisible(false);
        download_button->setEnabled(false);
    }
}

bool LLMSelectionDialog::openai_inputs_valid() const
{
    const QString key_text = openai_api_key_edit ? openai_api_key_edit->text().trimmed() : QString();
    const QString model_text = openai_model_edit ? openai_model_edit->text().trimmed() : QString();
    return !key_text.isEmpty() && !model_text.isEmpty();
}

bool LLMSelectionDialog::gemini_inputs_valid() const
{
    const QString key_text = gemini_api_key_edit ? gemini_api_key_edit->text().trimmed() : QString();
    const QString model_text = gemini_model_edit ? gemini_model_edit->text().trimmed() : QString();
    return !key_text.isEmpty() && !model_text.isEmpty();
}

void LLMSelectionDialog::update_local_choice_ui()
{
    if (!ok_button && button_box) {
        ok_button = button_box->button(QDialogButtonBox::Ok);
    }
    refresh_downloader();

    if (!downloader) {
        if (ok_button) ok_button->setEnabled(false);
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
        if (ok_button) {
            ok_button->setEnabled(true);
        }
        set_status_message(tr("Model ready."));
        break;
    case LLMDownloader::DownloadStatus::InProgress:
        progress_bar->setVisible(true);
        download_button->setVisible(true);
        download_button->setEnabled(!is_downloading.load());
        download_button->setText(tr("Resume download"));
        if (ok_button) {
            ok_button->setEnabled(false);
        }
        set_status_message(tr("Partial download detected. You can resume."));
        break;
    case LLMDownloader::DownloadStatus::NotStarted:
    default:
        progress_bar->setVisible(false);
        progress_bar->setValue(0);
        download_button->setVisible(true);
        download_button->setEnabled(!is_downloading.load());
        download_button->setText(tr("Download"));
        if (ok_button) {
            ok_button->setEnabled(false);
        }
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

    if (downloader->get_local_download_status() == LLMDownloader::DownloadStatus::InProgress) {
        try {
            downloader->init_if_needed();
        } catch (const std::exception& ex) {
            set_status_message(QString::fromStdString(ex.what()));
            downloader.reset();
        }
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

void LLMSelectionDialog::setup_visual_llm_download_entry(VisualLlmDownloadEntry& entry,
                                                     QWidget* parent,
                                                     const QString& title,
                                                     const std::string& env_var)
{
    entry.env_var = env_var;
    auto* group = new QGroupBox(title, parent);
    entry.container = group;

    auto* entry_layout = new QVBoxLayout(group);
    entry_layout->setSpacing(6);

    entry.remote_url_label = new QLabel(group);
    entry.remote_url_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    entry.local_path_label = new QLabel(group);
    entry.local_path_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    entry.file_size_label = new QLabel(group);
    entry.file_size_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    entry.status_label = new QLabel(group);
    entry.status_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    entry.progress_bar = new QProgressBar(group);
    entry.progress_bar->setRange(0, 100);
    entry.progress_bar->setValue(0);
    entry.progress_bar->setVisible(false);

    entry.download_button = new QPushButton(tr("Download"), group);
    entry.download_button->setEnabled(false);

    entry_layout->addWidget(entry.remote_url_label);
    entry_layout->addWidget(entry.local_path_label);
    entry_layout->addWidget(entry.file_size_label);
    entry_layout->addWidget(entry.status_label);
    entry_layout->addWidget(entry.progress_bar);
    entry_layout->addWidget(entry.download_button, 0, Qt::AlignLeft);
}

void LLMSelectionDialog::set_visual_status_message(VisualLlmDownloadEntry& entry, const QString& message)
{
    if (entry.status_label) {
        entry.status_label->setText(message);
    }
}

void LLMSelectionDialog::refresh_visual_llm_download_entry(VisualLlmDownloadEntry& entry)
{
    if (entry.env_var.empty()) {
        entry.downloader.reset();
        set_visual_status_message(entry, tr("Missing download URL environment variable."));
        if (entry.download_button) {
            entry.download_button->setEnabled(false);
        }
        return;
    }

    const char* env_url = std::getenv(entry.env_var.c_str());
    if (!env_url) {
        entry.downloader.reset();
        set_visual_status_message(entry,
                                  tr("Missing download URL environment variable (%1).")
                                      .arg(QString::fromStdString(entry.env_var)));
        if (entry.download_button) {
            entry.download_button->setEnabled(false);
        }
        return;
    }

    if (!entry.downloader) {
        entry.downloader = std::make_unique<LLMDownloader>(env_url);
    } else {
        entry.downloader->set_download_url(env_url);
    }

    if (entry.downloader->get_local_download_status() == LLMDownloader::DownloadStatus::InProgress) {
        try {
            entry.downloader->init_if_needed();
        } catch (const std::exception& ex) {
            set_visual_status_message(entry, QString::fromStdString(ex.what()));
            entry.downloader.reset();
        }
    }
}

void LLMSelectionDialog::update_visual_llm_download_entry(VisualLlmDownloadEntry& entry)
{
    if (!entry.downloader) {
        if (entry.progress_bar) {
            entry.progress_bar->setVisible(false);
        }
        if (entry.download_button) {
            entry.download_button->setVisible(true);
            entry.download_button->setEnabled(false);
        }
        return;
    }

    if (entry.remote_url_label) {
        entry.remote_url_label->setText(format_markup_label(tr("Remote URL"),
                                                            QString::fromStdString(entry.downloader->get_download_url()),
                                                            QStringLiteral("#1565c0")));
    }
    if (entry.local_path_label) {
        entry.local_path_label->setText(format_markup_label(tr("Local path"),
                                                            QString::fromStdString(entry.downloader->get_download_destination()),
                                                            QStringLiteral("#2e7d32")));
    }
    if (entry.file_size_label) {
        const auto size = entry.downloader->get_real_content_length();
        if (size > 0) {
            entry.file_size_label->setText(format_markup_label(tr("File size"),
                                                               QString::fromStdString(Utils::format_size(size)),
                                                               QStringLiteral("#333")));
        } else {
            entry.file_size_label->setText(tr("File size: unknown"));
        }
    }

    const auto status = entry.downloader->get_download_status();
    switch (status) {
    case LLMDownloader::DownloadStatus::Complete:
        if (entry.progress_bar) {
            entry.progress_bar->setVisible(true);
            entry.progress_bar->setValue(100);
        }
        if (entry.download_button) {
            entry.download_button->setEnabled(false);
            entry.download_button->setVisible(false);
        }
        set_visual_status_message(entry, tr("Model ready."));
        break;
    case LLMDownloader::DownloadStatus::InProgress:
        if (entry.progress_bar) {
            entry.progress_bar->setVisible(true);
        }
        if (entry.download_button) {
            entry.download_button->setVisible(true);
            entry.download_button->setEnabled(!entry.is_downloading.load());
            entry.download_button->setText(tr("Resume download"));
        }
        set_visual_status_message(entry, tr("Partial download detected. You can resume."));
        break;
    case LLMDownloader::DownloadStatus::NotStarted:
    default:
        if (entry.progress_bar) {
            entry.progress_bar->setVisible(false);
            entry.progress_bar->setValue(0);
        }
        if (entry.download_button) {
            entry.download_button->setVisible(true);
            entry.download_button->setEnabled(!entry.is_downloading.load());
            entry.download_button->setText(tr("Download"));
        }
        set_visual_status_message(entry, tr("Download required."));
        break;
    }
}

void LLMSelectionDialog::update_visual_llm_downloads()
{
    if (!visual_llm_download_section) {
        return;
    }

    refresh_visual_llm_download_entry(llava_model_download);
    update_visual_llm_download_entry(llava_model_download);

    refresh_visual_llm_download_entry(llava_mmproj_download);
    update_visual_llm_download_entry(llava_mmproj_download);
}

void LLMSelectionDialog::start_visual_llm_download(VisualLlmDownloadEntry& entry)
{
    if (!entry.downloader || entry.is_downloading.load()) {
        return;
    }

    bool network_available = Utils::is_network_available();
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (use_network_available_override_) {
        network_available = network_available_override_;
    }
#endif
    if (!network_available) {
        DialogUtils::show_error_dialog(this, ERR_NO_INTERNET_CONNECTION);
        return;
    }

    try {
        entry.downloader->init_if_needed();
    } catch (const std::exception& ex) {
        DialogUtils::show_error_dialog(this, ex.what());
        return;
    }

    entry.is_downloading = true;
    if (entry.download_button) {
        entry.download_button->setEnabled(false);
    }
    if (entry.progress_bar) {
        entry.progress_bar->setVisible(true);
        entry.progress_bar->setValue(0);
    }
    set_visual_status_message(entry, tr("Downloading…"));

    auto* entry_ptr = &entry;
    entry.downloader->start_download(
        [this, entry_ptr](double fraction) {
            QMetaObject::invokeMethod(this, [entry_ptr, fraction]() {
                if (entry_ptr->progress_bar) {
                    entry_ptr->progress_bar->setVisible(true);
                    entry_ptr->progress_bar->setValue(static_cast<int>(fraction * 100));
                }
            }, Qt::QueuedConnection);
        },
        [this, entry_ptr]() {
            QMetaObject::invokeMethod(this, [this, entry_ptr]() {
                entry_ptr->is_downloading = false;
                set_visual_status_message(*entry_ptr, tr("Download complete."));
                update_visual_llm_download_entry(*entry_ptr);
            }, Qt::QueuedConnection);
        },
        [this, entry_ptr](const std::string& text) {
            QMetaObject::invokeMethod(this, [this, entry_ptr, text]() {
                set_visual_status_message(*entry_ptr, QString::fromStdString(text));
            }, Qt::QueuedConnection);
        },
        [this, entry_ptr](const std::string& error_text) {
            QMetaObject::invokeMethod(this, [this, entry_ptr, error_text]() {
                entry_ptr->is_downloading = false;
                if (entry_ptr->progress_bar) {
                    entry_ptr->progress_bar->setVisible(false);
                }
                if (entry_ptr->download_button) {
                    entry_ptr->download_button->setEnabled(true);
                }

                const QString error = QString::fromStdString(error_text);
                if (error.compare(QStringLiteral("Download cancelled"), Qt::CaseInsensitive) == 0) {
                    set_visual_status_message(*entry_ptr, tr("Download cancelled."));
                } else {
                    set_visual_status_message(*entry_ptr, tr("Download error: %1").arg(error));
                }
            }, Qt::QueuedConnection);
        });
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

#if defined(AI_FILE_SORTER_TEST_BUILD)

LLMSelectionDialogTestAccess::VisualEntryRefs LLMSelectionDialogTestAccess::llava_model_entry(LLMSelectionDialog& dialog)
{
    return {
        dialog.llava_model_download.status_label,
        dialog.llava_model_download.download_button,
        dialog.llava_model_download.progress_bar,
        dialog.llava_model_download.downloader.get()
    };
}

LLMSelectionDialogTestAccess::VisualEntryRefs LLMSelectionDialogTestAccess::llava_mmproj_entry(LLMSelectionDialog& dialog)
{
    return {
        dialog.llava_mmproj_download.status_label,
        dialog.llava_mmproj_download.download_button,
        dialog.llava_mmproj_download.progress_bar,
        dialog.llava_mmproj_download.downloader.get()
    };
}

void LLMSelectionDialogTestAccess::refresh_visual_downloads(LLMSelectionDialog& dialog)
{
    dialog.update_visual_llm_downloads();
}

void LLMSelectionDialogTestAccess::update_llava_model_entry(LLMSelectionDialog& dialog)
{
    dialog.update_visual_llm_download_entry(dialog.llava_model_download);
}

void LLMSelectionDialogTestAccess::start_llava_model_download(LLMSelectionDialog& dialog)
{
    dialog.start_visual_llm_download(dialog.llava_model_download);
}

void LLMSelectionDialogTestAccess::set_network_available_override(LLMSelectionDialog& dialog,
                                                                   std::optional<bool> value)
{
    if (value.has_value()) {
        dialog.use_network_available_override_ = true;
        dialog.network_available_override_ = *value;
    } else {
        dialog.use_network_available_override_ = false;
    }
}

#endif // AI_FILE_SORTER_TEST_BUILD
