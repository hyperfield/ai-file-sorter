#include "LLMSelectionDialog.hpp"

#include "DialogUtils.hpp"
#include "LlmCatalog.hpp"
#include "ErrorMessages.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "CustomLLMDialog.hpp"
#include "CustomApiDialog.hpp"
#ifdef AI_FILE_SORTER_TEST_BUILD
#include "LLMSelectionDialogTestAccess.hpp"
#endif

#include <QButtonGroup>
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
#include <QToolButton>
#include <QScreen>
#include <QSizePolicy>
#include <QComboBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>
#include <QStyle>
#include <QVBoxLayout>
#include <QString>

#include <cstdlib>
#include <filesystem>


namespace {

QString format_markup_label(const QString& title, const QString& value, const QString& color)
{
    return QStringLiteral("<b>%1:</b> <span style=\"color:%2\">%3</span>")
        .arg(title, color, value.toHtmlEscaped());
}

long long local_file_size_or_zero(const std::string& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return 0;
    }
    return static_cast<long long>(size);
}

void set_fixed_progress_width(QProgressBar* bar, int multiplier)
{
    if (!bar || multiplier <= 0) {
        return;
    }
    const QSize hint = bar->sizeHint();
    int base_width = hint.width();
    if (base_width <= 0) {
        base_width = 120;
    }
    bar->setFixedWidth(base_width * multiplier);

    int base_height = hint.height();
    if (base_height <= 0) {
        base_height = std::max(12, bar->fontMetrics().height());
    }
    bar->setMinimumHeight(base_height);
    bar->setMaximumHeight(base_height);
}

void apply_progress_style(QProgressBar* bar)
{
    if (!bar) {
        return;
    }
#if defined(__APPLE__)
    bar->setTextVisible(false);
    bar->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        " border: 1px solid #d0d0d0;"
        " border-radius: 4px;"
        " background: #f4f4f4;"
        " }"
        "QProgressBar::chunk {"
        " background-color: #0a84ff;"
        " border-radius: 3px;"
        " }"));
#endif
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
    selected_custom_api_id = settings.get_active_custom_api_id();
    switch (selected_choice) {
    case LLMChoice::Remote_OpenAI:
        openai_radio->setChecked(true);
        break;
    case LLMChoice::Remote_Gemini:
        gemini_radio->setChecked(true);
        break;
    case LLMChoice::Remote_Custom:
        custom_api_radio->setChecked(true);
        break;
    case LLMChoice::Local_3b:
        local3_radio->setChecked(true);
        break;
    case LLMChoice::Local_3b_legacy:
        if (local3_legacy_radio && local3_legacy_radio->isVisible()) {
            local3_legacy_radio->setChecked(true);
        } else {
            local3_radio->setChecked(true);
            selected_choice = LLMChoice::Local_3b;
        }
        break;
    case LLMChoice::Local_7b:
        local7_radio->setChecked(true);
        break;
    case LLMChoice::Custom:
        custom_radio->setChecked(true);
        break;
    default:
        local3_radio->setChecked(true);
        selected_choice = LLMChoice::Local_3b;
        break;
    }
    refresh_custom_lists();
    refresh_custom_api_lists();
    if (selected_choice == LLMChoice::Custom) {
        select_custom_by_id(selected_custom_id);
    }
    if (selected_choice == LLMChoice::Remote_Custom) {
        select_custom_api_by_id(selected_custom_api_id);
    }

    update_ui_for_choice();
    adjust_dialog_size();
    const int min_width = 620;
    if (width() < min_width) {
        resize(min_width, height());
    }
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
    auto* outer_layout = new QVBoxLayout(this);
    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll_area_);
    auto* layout = new QVBoxLayout(content);

    auto* title = new QLabel(tr("Select LLM Mode"), this);
    title->setAlignment(Qt::AlignHCenter);
    layout->addWidget(title);

    layout->setSpacing(8);

    auto* radio_container = new QWidget(this);
    auto* radio_layout = new QVBoxLayout(radio_container);
    radio_layout->setSpacing(10);

    local7_radio = new QRadioButton(default_llm_label_for_choice(LLMChoice::Local_7b), radio_container);
    local7_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    auto* local7_desc = new QLabel(tr("Quite precise. Slower on CPU, but performs much better with GPU acceleration.\nSupports: Nvidia (CUDA), Apple (Metal), CPU."), radio_container);
    local7_desc->setWordWrap(true);

    local3_radio = new QRadioButton(default_llm_label_for_choice(LLMChoice::Local_3b), radio_container);
    local3_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    auto* local3_row = new QWidget(radio_container);
    auto* local3_row_layout = new QHBoxLayout(local3_row);
    local3_row_layout->setContentsMargins(0, 0, 0, 0);
    auto* local3_recommended = new QLabel(tr("Recommended"), local3_row);
    local3_recommended->setStyleSheet(QStringLiteral("color: #1f6feb; font-weight: 700;"));
    local3_row_layout->addWidget(local3_radio);
    local3_row_layout->addWidget(local3_recommended);
    local3_row_layout->addStretch(1);
    auto* local3_desc = new QLabel(tr("Less precise, but works quickly even on CPUs. Good for lightweight local use."), radio_container);
    local3_desc->setWordWrap(true);

    local3_legacy_radio = new QRadioButton(default_llm_label_for_choice(LLMChoice::Local_3b_legacy), radio_container);
    local3_legacy_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    local3_legacy_desc = new QLabel(tr("Legacy model kept for existing downloads."), radio_container);
    local3_legacy_desc->setWordWrap(true);
    const bool has_legacy_3b = legacy_local_3b_available();
    local3_legacy_radio->setVisible(has_legacy_3b);
    local3_legacy_desc->setVisible(has_legacy_3b);

    gemini_radio = new QRadioButton(tr("Gemini (Google AI Studio API key)"), radio_container);
    gemini_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
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
    auto* gemini_key_label = new QLabel(tr("Gemini API key"), gemini_inputs);
    gemini_key_label->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    gemini_form->addRow(gemini_key_label, gemini_key_row);

    gemini_model_edit = new QLineEdit(gemini_inputs);
    gemini_model_edit->setPlaceholderText(tr("e.g. gemini-2.5-flash-lite, gemini-2.5-flash, gemini-2.5-pro"));
    auto* gemini_model_label = new QLabel(tr("Model"), gemini_inputs);
    gemini_model_label->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    gemini_form->addRow(gemini_model_label, gemini_model_edit);

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
    openai_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
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
    auto* openai_key_label = new QLabel(tr("OpenAI API key"), openai_inputs);
    openai_key_label->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    openai_form->addRow(openai_key_label, openai_key_row);

    openai_model_edit = new QLineEdit(openai_inputs);
    openai_model_edit->setPlaceholderText(
        tr("e.g. gpt-4o-mini, gpt-4.1, o3-mini"));
    auto* openai_model_label = new QLabel(tr("Model"), openai_inputs);
    openai_model_label->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    openai_form->addRow(openai_model_label, openai_model_edit);

    openai_help_label = new QLabel(
        tr("Your key is stored locally in the config file for this device."),
        openai_inputs);
    openai_help_label->setWordWrap(true);
    openai_form->addRow(openai_help_label);
    openai_link_label = new QLabel(openai_inputs);
    openai_link_label->setTextFormat(Qt::RichText);
    openai_link_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    openai_link_label->setOpenExternalLinks(true);
    openai_link_label->setText(
        tr("<a href=\"https://platform.openai.com/api-keys\">Get an OpenAI API key</a>"));
    openai_form->addRow(openai_link_label);
    openai_inputs->setVisible(false);

    custom_api_radio = new QRadioButton(
        tr("Custom OpenAI-compatible API (advanced)"), radio_container);
    custom_api_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    auto* custom_api_desc = new QLabel(
        tr("Use OpenAI-compatible endpoints such as LM Studio or Ollama (local or remote)."),
        radio_container);
    custom_api_desc->setWordWrap(true);
    auto* custom_api_row = new QWidget(radio_container);
    auto* custom_api_layout = new QHBoxLayout(custom_api_row);
    custom_api_layout->setContentsMargins(24, 0, 0, 0);
    custom_api_combo = new QComboBox(custom_api_row);
    custom_api_combo->setMinimumContentsLength(10);
    custom_api_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    custom_api_combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    custom_api_combo->setFixedWidth(360);
    add_custom_api_button = new QPushButton(tr("Add…"), custom_api_row);
    edit_custom_api_button = new QPushButton(tr("Edit…"), custom_api_row);
    delete_custom_api_button = new QPushButton(tr("Delete"), custom_api_row);
    add_custom_api_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    edit_custom_api_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    delete_custom_api_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    custom_api_layout->addWidget(custom_api_combo);
    custom_api_layout->addWidget(add_custom_api_button);
    custom_api_layout->addWidget(edit_custom_api_button);
    custom_api_layout->addWidget(delete_custom_api_button);
    custom_api_layout->addStretch(1);

    custom_radio = new QRadioButton(
        tr("Custom local LLM (gguf)"), radio_container);
    custom_radio->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    auto* custom_row = new QWidget(radio_container);
    auto* custom_layout = new QHBoxLayout(custom_row);
    custom_layout->setContentsMargins(24, 0, 0, 0);
    custom_combo = new QComboBox(custom_row);
    custom_combo->setMinimumContentsLength(10);
    custom_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    custom_combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    custom_combo->setFixedWidth(360);
    add_custom_button = new QPushButton(tr("Add…"), custom_row);
    edit_custom_button = new QPushButton(tr("Edit…"), custom_row);
    delete_custom_button = new QPushButton(tr("Delete"), custom_row);
    add_custom_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    edit_custom_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    delete_custom_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    custom_layout->addWidget(custom_combo);
    custom_layout->addWidget(add_custom_button);
    custom_layout->addWidget(edit_custom_button);
    custom_layout->addWidget(delete_custom_button);
    custom_layout->addStretch(1);

    radio_layout->addWidget(local3_row);
    radio_layout->addWidget(local3_desc);
    radio_layout->addWidget(local7_radio);
    radio_layout->addWidget(local7_desc);
    radio_layout->addWidget(local3_legacy_radio);
    radio_layout->addWidget(local3_legacy_desc);
    radio_layout->addWidget(gemini_radio);
    radio_layout->addWidget(gemini_desc);
    radio_layout->addWidget(gemini_inputs);
    radio_layout->addWidget(openai_radio);
    radio_layout->addWidget(openai_desc);
    radio_layout->addWidget(openai_inputs);
    radio_layout->addWidget(custom_api_radio);
    radio_layout->addWidget(custom_api_desc);
    radio_layout->addWidget(custom_api_row);
    radio_layout->addWidget(custom_radio);
    radio_layout->addWidget(custom_row);

    auto* llm_group = new QButtonGroup(this);
    llm_group->setExclusive(true);
    llm_group->addButton(openai_radio);
    llm_group->addButton(gemini_radio);
    llm_group->addButton(custom_api_radio);
    llm_group->addButton(local3_radio);
    if (local3_legacy_radio) {
        llm_group->addButton(local3_legacy_radio);
    }
    llm_group->addButton(local7_radio);
    llm_group->addButton(custom_radio);

    layout->addWidget(radio_container);

    download_toggle_button = new QToolButton(this);
    download_toggle_button->setText(tr("Downloads"));
    download_toggle_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    download_toggle_button->setArrowType(Qt::RightArrow);
    download_toggle_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    download_toggle_button->setLayoutDirection(Qt::LeftToRight);
    download_toggle_button->setCheckable(true);
    download_toggle_button->setChecked(false);
    download_toggle_button->setStyleSheet(QStringLiteral("color: #1f6feb; font-weight: 600;"));
    auto* downloads_toggle_row = new QWidget(this);
    auto* downloads_toggle_layout = new QHBoxLayout(downloads_toggle_row);
    downloads_toggle_layout->setContentsMargins(0, 0, 0, 0);
    downloads_toggle_layout->addWidget(download_toggle_button);
    downloads_toggle_layout->addStretch(1);
    layout->addWidget(downloads_toggle_row);

    downloads_container = new QWidget(this);
    auto* downloads_layout = new QVBoxLayout(downloads_container);
    downloads_layout->setContentsMargins(0, 0, 0, 0);
    downloads_layout->setSpacing(10);

    download_section = new QWidget(downloads_container);
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
    progress_bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    set_fixed_progress_width(progress_bar, 3);
    apply_progress_style(progress_bar);
    progress_bar->setVisible(false);

    download_button = new QPushButton(tr("Download"), download_section);
    download_button->setEnabled(false);
    delete_download_button = new QPushButton(tr("Delete"), download_section);
    delete_download_button->setEnabled(false);

    auto* download_actions = new QWidget(download_section);
    auto* download_actions_layout = new QHBoxLayout(download_actions);
    download_actions_layout->setContentsMargins(0, 0, 0, 0);
    download_actions_layout->setSpacing(8);
    download_actions_layout->addWidget(download_button);
    download_actions_layout->addWidget(delete_download_button);
    download_actions_layout->addStretch(1);

    download_layout->addWidget(remote_url_label);
    download_layout->addWidget(local_path_label);
    download_layout->addWidget(file_size_label);
    download_layout->addWidget(status_label);
    download_layout->addWidget(progress_bar);
    download_layout->addWidget(download_actions, 0, Qt::AlignLeft);

    downloads_layout->addWidget(download_section);
    download_section->setVisible(false);

    visual_llm_download_section = new QGroupBox(tr("Image analysis models (LLaVA)"), downloads_container);
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

    downloads_layout->addWidget(visual_llm_download_section);
    layout->addWidget(downloads_container);
    downloads_container->setVisible(false);

    button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button = button_box->button(QDialogButtonBox::Ok);

    scroll_area_->setWidget(content);
    outer_layout->addWidget(scroll_area_);
    outer_layout->addWidget(button_box);
}


void LLMSelectionDialog::connect_signals()
{
    auto update_handler = [this]() { update_ui_for_choice(); };
    connect(openai_radio, &QRadioButton::toggled, this, update_handler);
    connect(gemini_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_api_radio, &QRadioButton::toggled, this, update_handler);
    connect(local3_radio, &QRadioButton::toggled, this, update_handler);
    if (local3_legacy_radio) {
        connect(local3_legacy_radio, &QRadioButton::toggled, this, update_handler);
    }
    connect(local7_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_radio, &QRadioButton::toggled, this, update_handler);
    connect(custom_combo, &QComboBox::currentTextChanged, this, update_handler);
    connect(custom_api_combo, &QComboBox::currentTextChanged, this, update_handler);
    connect(openai_api_key_edit, &QLineEdit::textChanged, this, update_handler);
    connect(openai_model_edit, &QLineEdit::textChanged, this, update_handler);
    connect(gemini_api_key_edit, &QLineEdit::textChanged, this, update_handler);
    connect(gemini_model_edit, &QLineEdit::textChanged, this, update_handler);
    connect(add_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_add_custom);
    connect(edit_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_edit_custom);
    connect(delete_custom_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_delete_custom);
    connect(add_custom_api_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_add_custom_api);
    connect(edit_custom_api_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_edit_custom_api);
    connect(delete_custom_api_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_delete_custom_api);
    if (delete_download_button) {
        connect(delete_download_button, &QPushButton::clicked, this, &LLMSelectionDialog::handle_delete_download);
    }
    if (download_toggle_button) {
        connect(download_toggle_button, &QToolButton::toggled, this, [this](bool checked) {
            if (downloads_container) {
                downloads_container->setVisible(checked);
            }
            download_toggle_button->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
            adjust_dialog_size();
        });
    }

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

void LLMSelectionDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        adjust_dialog_size();
    });
}


LLMChoice LLMSelectionDialog::get_selected_llm_choice() const
{
    return selected_choice;
}

std::string LLMSelectionDialog::get_selected_custom_llm_id() const
{
    return selected_custom_id;
}

std::string LLMSelectionDialog::get_selected_custom_api_id() const
{
    return selected_custom_api_id;
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

bool LLMSelectionDialog::legacy_local_3b_available() const
{
    const char* env_url = std::getenv("LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL");
    if (!env_url || *env_url == '\0') {
        return false;
    }
    std::string path;
    try {
        path = Utils::make_default_path_to_file_from_download_url(env_url);
    } catch (...) {
        return false;
    }
    std::error_code ec;
    return !path.empty() && std::filesystem::exists(path, ec);
}

void LLMSelectionDialog::update_legacy_local_3b_visibility()
{
    if (!local3_legacy_radio || !local3_legacy_desc) {
        return;
    }
    const bool available = legacy_local_3b_available();
    local3_legacy_radio->setVisible(available);
    local3_legacy_desc->setVisible(available);
    if (!available && local3_legacy_radio->isChecked() && local3_radio) {
        local3_radio->setChecked(true);
    }
}

void LLMSelectionDialog::update_ui_for_choice()
{
    update_legacy_local_3b_visibility();
    update_custom_buttons();
    update_custom_api_buttons();

    update_radio_selection();
    update_custom_choice_ui();
    update_custom_api_choice_ui();
    update_visual_llm_downloads();

    const bool is_local_builtin = (selected_choice == LLMChoice::Local_3b
        || selected_choice == LLMChoice::Local_3b_legacy
        || selected_choice == LLMChoice::Local_7b);

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
    } else if (custom_api_radio->isChecked()) {
        selected_choice = LLMChoice::Remote_Custom;
    } else if (local3_legacy_radio && local3_legacy_radio->isChecked()) {
        selected_choice = LLMChoice::Local_3b_legacy;
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
    const bool is_local_builtin = (selected_choice == LLMChoice::Local_3b
        || selected_choice == LLMChoice::Local_3b_legacy
        || selected_choice == LLMChoice::Local_7b);
    const bool is_remote_openai = selected_choice == LLMChoice::Remote_OpenAI;
    const bool is_remote_gemini = selected_choice == LLMChoice::Remote_Gemini;
    const bool is_remote_custom = selected_choice == LLMChoice::Remote_Custom;
    const bool is_custom = selected_choice == LLMChoice::Custom;
    if (download_toggle_button) {
        download_toggle_button->setVisible(is_local_builtin);
        if (!is_local_builtin) {
            download_toggle_button->setChecked(false);
        }
    }
    if (downloads_container) {
        const bool show_downloads = is_local_builtin && download_toggle_button
            && download_toggle_button->isChecked();
        downloads_container->setVisible(show_downloads);
    }
    download_section->setVisible(is_local_builtin);
    adjust_dialog_size();
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

    if (is_remote_custom) {
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

void LLMSelectionDialog::update_custom_api_choice_ui()
{
    if (!ok_button && button_box) {
        ok_button = button_box->button(QDialogButtonBox::Ok);
    }

    const bool is_custom_api = selected_choice == LLMChoice::Remote_Custom;
    if (custom_api_combo) {
        custom_api_combo->setEnabled(is_custom_api);
    }
    if (edit_custom_api_button) {
        edit_custom_api_button->setEnabled(is_custom_api && custom_api_combo
            && custom_api_combo->currentIndex() >= 0 && custom_api_combo->count() > 0);
    }
    if (delete_custom_api_button) {
        delete_custom_api_button->setEnabled(is_custom_api && custom_api_combo
            && custom_api_combo->currentIndex() >= 0 && custom_api_combo->count() > 0);
    }

    if (!is_custom_api) {
        return;
    }

    if (custom_api_combo && custom_api_combo->currentIndex() >= 0) {
        selected_custom_api_id = custom_api_combo->currentData().toString().toStdString();
    } else {
        selected_custom_api_id.clear();
    }

    if (ok_button) {
        ok_button->setEnabled(!selected_custom_api_id.empty());
    }
    if (progress_bar) {
        progress_bar->setVisible(false);
    }
    if (download_button) {
        download_button->setVisible(false);
    }
    set_status_message(selected_custom_api_id.empty()
        ? tr("Choose or add a custom API endpoint.")
        : tr("Custom API selected."));
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
        if (delete_download_button) {
            delete_download_button->setVisible(true);
            delete_download_button->setEnabled(true);
        }
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
        if (delete_download_button) {
            delete_download_button->setVisible(true);
            delete_download_button->setEnabled(true);
        }
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
        if (delete_download_button) {
            delete_download_button->setVisible(true);
            delete_download_button->setEnabled(false);
        }
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

void LLMSelectionDialog::handle_delete_download()
{
    if (!downloader) {
        return;
    }

    const std::string path = downloader->get_download_destination();
    if (path.empty()) {
        return;
    }

    const QString model_label = default_llm_label_for_choice(selected_choice);
    const QString title = tr("Delete downloaded model?");
    const QString prompt = tr("Delete the downloaded model %1?").arg(model_label);
    const auto answer = QMessageBox::question(this, title, prompt, QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    downloader->cancel_download();
    is_downloading.store(false);

    std::error_code ec;
    bool removed_any = false;
    if (std::filesystem::exists(path, ec)) {
        removed_any = std::filesystem::remove(path, ec) || removed_any;
    }
    const std::string meta_path = path + ".aifs.meta";
    if (std::filesystem::exists(meta_path, ec)) {
        removed_any = std::filesystem::remove(meta_path, ec) || removed_any;
    }

    if (ec) {
        set_status_message(tr("Failed to delete downloaded model."));
    } else if (removed_any) {
        set_status_message(tr("Deleted downloaded model."));
    } else {
        set_status_message(tr("No downloaded model found to delete."));
    }

    refresh_downloader();
    update_download_info();
    update_local_choice_ui();
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

    long long size = downloader->get_real_content_length();
    if (size <= 0 && downloader->get_local_download_status() == LLMDownloader::DownloadStatus::Complete) {
        size = local_file_size_or_zero(downloader->get_download_destination());
    }
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

void LLMSelectionDialog::refresh_custom_api_lists()
{
    if (!custom_api_combo) {
        return;
    }

    custom_api_combo->blockSignals(true);
    custom_api_combo->clear();
    for (const auto& entry : settings.get_custom_api_endpoints()) {
        custom_api_combo->addItem(QString::fromStdString(entry.name),
                                  QString::fromStdString(entry.id));
    }
    if (!selected_custom_api_id.empty()) {
        select_custom_api_by_id(selected_custom_api_id);
    } else if (custom_api_combo->count() > 0) {
        custom_api_combo->setCurrentIndex(0);
        selected_custom_api_id = custom_api_combo->currentData().toString().toStdString();
    }
    custom_api_combo->blockSignals(false);
    update_custom_api_buttons();
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

void LLMSelectionDialog::select_custom_api_by_id(const std::string& id)
{
    if (!custom_api_combo) {
        return;
    }
    for (int i = 0; i < custom_api_combo->count(); ++i) {
        if (custom_api_combo->itemData(i).toString().toStdString() == id) {
            custom_api_combo->setCurrentIndex(i);
            return;
        }
    }
    if (custom_api_combo->count() > 0) {
        custom_api_combo->setCurrentIndex(0);
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

void LLMSelectionDialog::handle_add_custom_api()
{
    CustomApiDialog editor(this);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    CustomApiEndpoint entry = editor.result();
    selected_custom_api_id = settings.upsert_custom_api_endpoint(entry);
    refresh_custom_api_lists();
    select_custom_api_by_id(selected_custom_api_id);
    custom_api_radio->setChecked(true);
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

void LLMSelectionDialog::handle_edit_custom_api()
{
    if (!custom_api_combo || custom_api_combo->currentIndex() < 0) {
        return;
    }
    const std::string id = custom_api_combo->currentData().toString().toStdString();
    CustomApiEndpoint entry = settings.find_custom_api_endpoint(id);
    if (entry.id.empty()) {
        return;
    }

    CustomApiDialog editor(this, entry);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    CustomApiEndpoint updated = editor.result();
    updated.id = entry.id;
    selected_custom_api_id = settings.upsert_custom_api_endpoint(updated);
    refresh_custom_api_lists();
    select_custom_api_by_id(selected_custom_api_id);
    custom_api_radio->setChecked(true);
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

void LLMSelectionDialog::handle_delete_custom_api()
{
    if (!custom_api_combo || custom_api_combo->currentIndex() < 0) {
        return;
    }
    const std::string id = custom_api_combo->currentData().toString().toStdString();
    const QString name = custom_api_combo->currentText();
    const auto response = QMessageBox::question(this,
                                                tr("Delete custom API"),
                                                tr("Remove '%1' from your custom API list? This does not affect the server.")
                                                    .arg(name));
    if (response != QMessageBox::Yes) {
        return;
    }
    settings.remove_custom_api_endpoint(id);
    if (selected_custom_api_id == id) {
        selected_custom_api_id.clear();
    }
    refresh_custom_api_lists();
    custom_api_radio->setChecked(custom_api_combo->count() > 0);
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

void LLMSelectionDialog::update_custom_api_buttons()
{
    const bool has_selection = custom_api_combo && custom_api_combo->currentIndex() >= 0 && custom_api_combo->count() > 0;
    if (edit_custom_api_button) {
        edit_custom_api_button->setEnabled(has_selection && custom_api_radio->isChecked());
    }
    if (delete_custom_api_button) {
        delete_custom_api_button->setEnabled(has_selection && custom_api_radio->isChecked());
    }
}

void LLMSelectionDialog::setup_visual_llm_download_entry(VisualLlmDownloadEntry& entry,
                                                     QWidget* parent,
                                                     const QString& title,
                                                     const std::string& env_var)
{
    entry.env_var = env_var;
    entry.display_name = title.toStdString();
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
    entry.progress_bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    set_fixed_progress_width(entry.progress_bar, 3);
    apply_progress_style(entry.progress_bar);
    entry.progress_bar->setVisible(false);

    entry.download_button = new QPushButton(tr("Download"), group);
    entry.download_button->setEnabled(false);
    entry.delete_button = new QPushButton(tr("Delete"), group);
    entry.delete_button->setEnabled(false);

    auto* action_row = new QWidget(group);
    auto* action_layout = new QHBoxLayout(action_row);
    action_layout->setContentsMargins(0, 0, 0, 0);
    action_layout->setSpacing(8);
    action_layout->addWidget(entry.download_button);
    action_layout->addWidget(entry.delete_button);
    action_layout->addStretch(1);

    entry_layout->addWidget(entry.remote_url_label);
    entry_layout->addWidget(entry.local_path_label);
    entry_layout->addWidget(entry.file_size_label);
    entry_layout->addWidget(entry.status_label);
    entry_layout->addWidget(entry.progress_bar);
    entry_layout->addWidget(action_row, 0, Qt::AlignLeft);

    if (entry.delete_button) {
        auto* entry_ptr = &entry;
        connect(entry.delete_button, &QPushButton::clicked, this, [this, entry_ptr]() {
            handle_delete_visual_download(*entry_ptr);
        });
    }
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
        if (entry.delete_button) {
            entry.delete_button->setVisible(true);
            entry.delete_button->setEnabled(false);
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
    const auto local_status = entry.downloader->get_local_download_status();
    if (entry.file_size_label) {
        long long size = entry.downloader->get_real_content_length();
        if (size <= 0 && local_status == LLMDownloader::DownloadStatus::Complete) {
            size = local_file_size_or_zero(entry.downloader->get_download_destination());
        }
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
        if (entry.delete_button) {
            entry.delete_button->setVisible(true);
            entry.delete_button->setEnabled(true);
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
        if (entry.delete_button) {
            entry.delete_button->setVisible(true);
            entry.delete_button->setEnabled(true);
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
        if (entry.delete_button) {
            entry.delete_button->setVisible(true);
            entry.delete_button->setEnabled(false);
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

void LLMSelectionDialog::handle_delete_visual_download(VisualLlmDownloadEntry& entry)
{
    if (!entry.downloader) {
        return;
    }

    const std::string path = entry.downloader->get_download_destination();
    if (path.empty()) {
        return;
    }

    const QString display_name = QString::fromStdString(entry.display_name);
    const QString title = tr("Delete downloaded model?");
    const QString prompt = tr("Delete the downloaded model %1?").arg(display_name);
    const auto answer = QMessageBox::question(this, title, prompt, QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    entry.downloader->cancel_download();
    entry.is_downloading.store(false);

    std::error_code ec;
    bool removed_any = false;
    if (std::filesystem::exists(path, ec)) {
        removed_any = std::filesystem::remove(path, ec) || removed_any;
    }
    const std::string meta_path = path + ".aifs.meta";
    if (std::filesystem::exists(meta_path, ec)) {
        removed_any = std::filesystem::remove(meta_path, ec) || removed_any;
    }

    if (ec) {
        set_visual_status_message(entry, tr("Failed to delete downloaded model."));
    } else if (removed_any) {
        set_visual_status_message(entry, tr("Deleted downloaded model."));
    } else {
        set_visual_status_message(entry, tr("No downloaded model found to delete."));
    }

    refresh_visual_llm_download_entry(entry);
    update_visual_llm_download_entry(entry);
}

void LLMSelectionDialog::adjust_dialog_size()
{
    if (!scroll_area_) {
        return;
    }
    auto* widget = scroll_area_->widget();
    if (!widget) {
        return;
    }

    widget->adjustSize();
    const QSize content_hint = widget->sizeHint();
    const int content_height = content_hint.height();
    const int content_width = content_hint.width();

    int scroll_height = content_height;
    const int button_height = button_box ? button_box->sizeHint().height() : 0;
    const int button_width = button_box ? button_box->sizeHint().width() : 0;

    QMargins margins;
    int spacing = 0;
    if (layout()) {
        margins = layout()->contentsMargins();
        spacing = layout()->spacing();
    }

    const int frame = scroll_area_->frameWidth() * 2;
    int desired_width = std::max(content_width + frame, button_width);
    desired_width += margins.left() + margins.right();
    int desired_height = scroll_height + button_height + margins.top() + margins.bottom();
    if (button_height > 0) {
        desired_height += spacing;
    }

    if (const QScreen* screen = this->screen()) {
        const QSize available = screen->availableGeometry().size();
        const int max_width = static_cast<int>(available.width() * 0.8);
        const int max_height = static_cast<int>(available.height() * 0.8);
        const int max_scroll_height = std::max(
            0,
            max_height - (button_height + margins.top() + margins.bottom() + (button_height > 0 ? spacing : 0)));
        int target_height = desired_height;
        const bool needs_scroll = target_height > max_height;
        if (needs_scroll) {
            scroll_area_->setMinimumHeight(0);
            scroll_area_->setMaximumHeight(max_scroll_height);
            target_height = max_height;
        } else {
            scroll_area_->setMinimumHeight(scroll_height);
            scroll_area_->setMaximumHeight(scroll_height);
        }
        int scrollbar_padding = 0;
        if (needs_scroll) {
            scrollbar_padding = scroll_area_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, scroll_area_);
        }
        const int min_width = 620;
        int target_width = std::min(desired_width + scrollbar_padding, max_width);
        target_width = std::max(target_width, min_width);
        resize(target_width, target_height);
    } else {
        scroll_area_->setMinimumHeight(scroll_height);
        scroll_area_->setMaximumHeight(scroll_height);
        adjustSize();
    }

    widget->updateGeometry();
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
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
    if (selected_choice == LLMChoice::Local_3b_legacy) {
        return "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL";
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
