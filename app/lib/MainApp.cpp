#include "MainApp.hpp"

#include "CategorizationSession.hpp"
#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "LLMClient.hpp"
#include "GeminiClient.hpp"
#include "LLMSelectionDialog.hpp"
#include "Logger.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "Updater.hpp"
#include "TranslationManager.hpp"
#include "Utils.hpp"
#include "Types.hpp"
#include "CategoryLanguage.hpp"
#include "MainAppUiBuilder.hpp"
#include "UiTranslator.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "DocumentTextAnalyzer.hpp"
#include "WhitelistManagerDialog.hpp"
#include "UndoManager.hpp"
#ifdef AI_FILE_SORTER_TEST_BUILD
#include "MainAppTestAccess.hpp"
#endif

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QFile>
#include <QHeaderView>
#include <QKeySequence>
#include <QByteArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QCoreApplication>
#include <QStatusBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QDialog>
#include <QWidget>
#include <QIcon>
#include <QDir>
#include <QStyle>
#include <QEvent>
#include <QStackedWidget>
#include <QThread>

#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <optional>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <LocalLLMClient.hpp>

using namespace std::chrono_literals;

namespace {

void schedule_next_support_prompt(Settings& settings, int total_files, int increment) {
    if (increment <= 0) {
        increment = 100;
    }
    settings.set_next_support_prompt_threshold(total_files + increment);
    settings.save();
}

void maybe_show_support_prompt(Settings& settings,
                               bool& prompt_active,
                               std::function<MainApp::SupportPromptResult(int)> show_prompt) {
    if (prompt_active) {
        return;
    }

    const int total = settings.get_total_categorized_files();
    int threshold = settings.get_next_support_prompt_threshold();
    if (threshold <= 0) {
        const int base = std::max(total, 0);
        threshold = ((base / 100) + 1) * 100;
        settings.set_next_support_prompt_threshold(threshold);
        settings.save();
    }

    if (total < threshold || total == 0) {
        return;
    }

    prompt_active = true;
    MainApp::SupportPromptResult result = MainApp::SupportPromptResult::NotSure;
    if (show_prompt) {
        result = show_prompt(total);
    }
    prompt_active = false;

    int increment = 100;
    if (result == MainApp::SupportPromptResult::Support) {
        increment = 400;
    } else if (result == MainApp::SupportPromptResult::CannotDonate) {
        increment = 200;
    }

    schedule_next_support_prompt(settings, total, increment);
}

void record_categorized_metrics_impl(Settings& settings,
                                     bool& prompt_active,
                                     int count,
                                     std::function<MainApp::SupportPromptResult(int)> show_prompt) {
    if (count <= 0) {
        return;
    }

    settings.add_categorized_files(count);
    settings.save();
    maybe_show_support_prompt(settings, prompt_active, std::move(show_prompt));
}

std::string to_utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

struct VisualLlmPaths {
    std::filesystem::path model_path;
    std::filesystem::path mmproj_path;
};

std::optional<std::filesystem::path> resolve_mmproj_path(const std::filesystem::path& primary) {
    if (std::filesystem::exists(primary)) {
        return primary;
    }

    const auto llm_dir = std::filesystem::path(Utils::get_default_llm_destination());
    static const char* kAltMmprojNames[] = {
        "mmproj-model-f16.gguf",
        "llava-v1.6-mistral-7b-mmproj-f16.gguf"
    };
    for (const char* alt_name : kAltMmprojNames) {
        const auto candidate = llm_dir / alt_name;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

std::optional<VisualLlmPaths> resolve_visual_llm_paths(std::string* error) {
    const char* model_url = std::getenv("LLAVA_MODEL_URL");
    const char* mmproj_url = std::getenv("LLAVA_MMPROJ_URL");
    if (!model_url || !*model_url || !mmproj_url || !*mmproj_url) {
        if (error) {
            *error = "Missing visual LLM download URLs. Check LLAVA_MODEL_URL and LLAVA_MMPROJ_URL.";
        }
        return std::nullopt;
    }

    const auto model_path = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(model_url));
    if (!std::filesystem::exists(model_path)) {
        if (error) {
            *error = "Visual LLM model file is missing: " + model_path.string();
        }
        return std::nullopt;
    }

    const auto mmproj_primary = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(mmproj_url));
    const auto mmproj_path = resolve_mmproj_path(mmproj_primary);
    if (!mmproj_path) {
        if (error) {
            *error = "Visual LLM mmproj file is missing: " + mmproj_primary.string();
        }
        return std::nullopt;
    }

    return VisualLlmPaths{model_path, *mmproj_path};
}

bool should_use_visual_gpu() {
    const char* backend = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
    if (!backend || !*backend) {
        return true;
    }
    std::string value = backend;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value != "cpu";
}

std::optional<bool> read_env_bool(const char* key) {
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<int> read_env_int(const char* key) {
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }
    char* end_ptr = nullptr;
    long parsed = std::strtol(value, &end_ptr, 10);
    if (end_ptr == value || (end_ptr && *end_ptr != '\0')) {
        return std::nullopt;
    }
    if (parsed <= 0 || parsed > 100000) {
        return std::nullopt;
    }
    return static_cast<int>(parsed);
}

int resolve_local_context_tokens() {
    if (auto parsed = read_env_int("AI_FILE_SORTER_CTX_TOKENS")) {
        return *parsed;
    }
    if (auto parsed = read_env_int("LLAMA_CPP_MAX_CONTEXT")) {
        return *parsed;
    }
    return 2048;
}

size_t resolve_document_char_budget(bool using_local_llm, int max_output_tokens) {
    int context_tokens = using_local_llm ? resolve_local_context_tokens() : 4096;
    context_tokens = std::clamp(context_tokens, 512, 8192);
    const int reserve_tokens = std::max(192, context_tokens / 6);
    const int output_tokens = std::clamp(max_output_tokens, 0, context_tokens / 2);
    const int prompt_tokens = std::max(256, context_tokens - reserve_tokens - output_tokens);
    const size_t chars_per_token = using_local_llm ? 2 : 4;
    return static_cast<size_t>(prompt_tokens) * chars_per_token;
}

void split_entries_for_analysis(const std::vector<FileEntry>& files,
                                bool analyze_images,
                                bool analyze_documents,
                                bool process_images_only,
                                bool process_documents_only,
                                bool rename_images_only,
                                bool rename_documents_only,
                                const std::unordered_set<std::string>& renamed_files,
                                std::vector<FileEntry>& image_entries,
                                std::vector<FileEntry>& document_entries,
                                std::vector<FileEntry>& other_entries) {
    image_entries.clear();
    document_entries.clear();
    other_entries.clear();
    image_entries.reserve(files.size());
    document_entries.reserve(files.size());
    other_entries.reserve(files.size());

    const bool restrict_types = process_images_only || process_documents_only;
    const bool allow_images = !restrict_types || process_images_only;
    const bool allow_documents = !restrict_types || process_documents_only;

    for (const auto& entry : files) {
        const bool is_image_entry = entry.type == FileType::File &&
                                    LlavaImageAnalyzer::is_supported_image(entry.full_path);
        const bool is_document_entry = entry.type == FileType::File &&
                                       DocumentTextAnalyzer::is_supported_document(entry.full_path);

        if (is_image_entry) {
            if (!allow_images) {
                continue;
            }
            if (analyze_images) {
                const bool already_renamed = renamed_files.contains(entry.file_name);
                if (already_renamed) {
                    if (rename_images_only) {
                        continue;
                    }
                    // Already-renamed images skip vision analysis and use filename/path categorization.
                    other_entries.push_back(entry);
                } else {
                    image_entries.push_back(entry);
                }
            } else if (!restrict_types) {
                other_entries.push_back(entry);
            }
            continue;
        }

        if (is_document_entry) {
            if (!allow_documents) {
                continue;
            }
            if (analyze_documents) {
                const bool already_renamed = renamed_files.contains(entry.file_name);
                if (already_renamed) {
                    if (rename_documents_only) {
                        continue;
                    }
                    // Already-renamed documents skip content analysis and use filename/path categorization.
                    other_entries.push_back(entry);
                } else {
                    document_entries.push_back(entry);
                }
            } else if (!restrict_types) {
                other_entries.push_back(entry);
            }
            continue;
        }

        if (!restrict_types) {
            other_entries.push_back(entry);
        }
    }
}

} // namespace

MainApp::MainApp(Settings& settings, bool development_mode, QWidget* parent)
    : QMainWindow(parent),
      settings(settings),
      db_manager(settings.get_config_dir()),
      core_logger(Logger::get_logger("core_logger")),
      ui_logger(Logger::get_logger("ui_logger")),
      whitelist_store(settings.get_config_dir()),
      categorization_service(settings, db_manager, core_logger),
      consistency_pass_service(db_manager, core_logger),
      results_coordinator(dirscanner),
      undo_manager_(settings.get_config_dir() + "/undo"),
      development_mode_(development_mode),
      development_prompt_logging_enabled_(development_mode ? settings.get_development_prompt_logging() : false)
{
    TranslationManager::instance().initialize_for_app(qApp, settings.get_language());
    initialize_whitelists();

    using_local_llm = !is_remote_choice(settings.get_llm_choice());

    MainAppUiBuilder ui_builder;
    ui_builder.build(*this);
    ui_translator_ = std::make_unique<UiTranslator>(ui_builder.build_translator_dependencies(*this));
    retranslate_ui();
    setup_file_explorer();
    connect_signals();
    connect_edit_actions();
#if !defined(AI_FILE_SORTER_TEST_BUILD)
    start_updater();
#endif
    load_settings();
    set_app_icon();
}


MainApp::~MainApp() = default;


void MainApp::run()
{
    show();
}


void MainApp::shutdown()
{
    stop_running_analysis();
    save_settings();
}


void MainApp::setup_file_explorer()
{
    create_file_explorer_dock();
    setup_file_system_model();
    setup_file_explorer_view();
    connect_file_explorer_signals();
    apply_file_explorer_preferences();
}

void MainApp::create_file_explorer_dock()
{
    file_explorer_dock = new QDockWidget(tr("File Explorer"), this);
    file_explorer_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, file_explorer_dock);
}

void MainApp::setup_file_system_model()
{
    if (!file_explorer_dock) {
        return;
    }

    file_system_model = new QFileSystemModel(file_explorer_dock);
    file_system_model->setRootPath(QDir::rootPath());
    file_system_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Drives | QDir::AllDirs);
}

void MainApp::setup_file_explorer_view()
{
    if (!file_explorer_dock || !file_system_model) {
        return;
    }

    file_explorer_view = new QTreeView(file_explorer_dock);
    file_explorer_view->setModel(file_system_model);
    const QString root_path = file_system_model->rootPath();
    file_explorer_view->setRootIndex(file_system_model->index(root_path));

    const QModelIndex home_index = file_system_model->index(QDir::homePath());
    if (home_index.isValid()) {
        file_explorer_view->setCurrentIndex(home_index);
        file_explorer_view->scrollTo(home_index);
    }

    file_explorer_view->setHeaderHidden(false);
    file_explorer_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    file_explorer_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    file_explorer_view->setColumnHidden(1, true);
    file_explorer_view->setColumnHidden(2, true);
    file_explorer_view->setColumnHidden(3, true);
    file_explorer_view->setExpandsOnDoubleClick(true);

    file_explorer_dock->setWidget(file_explorer_view);
}

void MainApp::connect_file_explorer_signals()
{
    if (!file_explorer_view || !file_explorer_view->selectionModel()) {
        return;
    }

    connect(file_explorer_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                if (!file_system_model || suppress_explorer_sync_) {
                    return;
                }
                if (!current.isValid() || !file_system_model->isDir(current)) {
                    return;
                }
                const QString path = file_system_model->filePath(current);
                if (path_entry && path_entry->text() == path) {
                    update_folder_contents(path);
                } else {
                    on_directory_selected(path, true);
                }
            });

    if (file_explorer_dock) {
        connect(file_explorer_dock, &QDockWidget::visibilityChanged, this, [this](bool) {
            update_results_view_mode();
        });
    }
}

void MainApp::apply_file_explorer_preferences()
{
    if (!file_explorer_dock) {
        return;
    }

    const bool show_explorer = settings.get_show_file_explorer();
    if (file_explorer_menu_action) {
        file_explorer_menu_action->setChecked(show_explorer);
    }
    if (consistency_pass_action) {
        consistency_pass_action->setChecked(settings.get_consistency_pass_enabled());
    }

    file_explorer_dock->setVisible(show_explorer);
    update_results_view_mode();
}


void MainApp::connect_signals()
{
    connect(analyze_button, &QPushButton::clicked, this, &MainApp::on_analyze_clicked);
    connect(browse_button, &QPushButton::clicked, this, [this]() {
        const QString directory = QFileDialog::getExistingDirectory(this, tr("Select Directory"), path_entry->text());
        if (!directory.isEmpty()) {
            on_directory_selected(directory);
        }
    });

    connect(path_entry, &QLineEdit::returnPressed, this, [this]() {
        const QString folder = path_entry->text();
        if (QDir(folder).exists()) {
            on_directory_selected(folder);
        } else {
            show_error_dialog(ERR_INVALID_PATH);
        }
    });

    connect_folder_contents_signals();
    connect_checkbox_signals();
    connect_whitelist_signals();
}

void MainApp::connect_folder_contents_signals()
{
    if (!folder_contents_view || !folder_contents_model || !folder_contents_view->selectionModel()) {
        return;
    }
    folder_contents_view->setExpandsOnDoubleClick(true);

    connect(folder_contents_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                if (suppress_folder_view_sync_) {
                    return;
                }
                if (!folder_contents_model || !current.isValid()) {
                    return;
                }
                if (!folder_contents_model->isDir(current)) {
                    return;
                }
                on_directory_selected(folder_contents_model->filePath(current), true);
            });

    connect(folder_contents_model, &QFileSystemModel::directoryLoaded,
            this, [this](const QString& path) {
                if (!folder_contents_view || !folder_contents_model) {
                    return;
                }
                if (folder_contents_model->rootPath() == path) {
                    folder_contents_view->resizeColumnToContents(0);
                }
            });
}

void MainApp::connect_checkbox_signals()
{
    connect(use_subcategories_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        settings.set_use_subcategories(checked);
        if (categorization_dialog) {
            categorization_dialog->set_show_subcategory_column(checked);
        }
    });

    if (categorization_style_refined_radio) {
        connect(categorization_style_refined_radio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                set_categorization_style(false);
                settings.set_use_consistency_hints(false);
            } else if (categorization_style_consistent_radio &&
                       !categorization_style_consistent_radio->isChecked()) {
                set_categorization_style(true);
                settings.set_use_consistency_hints(true);
            }
        });
    }

    if (categorization_style_consistent_radio) {
        connect(categorization_style_consistent_radio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                set_categorization_style(true);
                settings.set_use_consistency_hints(true);
            } else if (categorization_style_refined_radio &&
                       !categorization_style_refined_radio->isChecked()) {
                set_categorization_style(false);
                settings.set_use_consistency_hints(false);
            }
        });
    }

    connect(categorize_files_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        ensure_one_checkbox_active(categorize_files_checkbox);
        update_file_scan_option(FileScanOptions::Files, checked);
        settings.set_categorize_files(checked);
    });

    connect(categorize_directories_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        ensure_one_checkbox_active(categorize_directories_checkbox);
        update_file_scan_option(FileScanOptions::Directories, checked);
        settings.set_categorize_directories(checked);
    });

    if (analyze_images_checkbox) {
        connect(analyze_images_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            handle_image_analysis_toggle(checked);
        });
    }

    if (process_images_only_checkbox) {
        connect(process_images_only_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            settings.set_process_images_only(checked);
            update_image_only_controls();
        });
    }

    if (offer_rename_images_checkbox) {
        connect(offer_rename_images_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (!checked && rename_images_only_checkbox && rename_images_only_checkbox->isChecked()) {
                QSignalBlocker blocker(rename_images_only_checkbox);
                rename_images_only_checkbox->setChecked(false);
            }
            settings.set_offer_rename_images(checked);
            if (rename_images_only_checkbox) {
                settings.set_rename_images_only(rename_images_only_checkbox->isChecked());
            }
            update_image_analysis_controls();
        });
    }

    if (rename_images_only_checkbox) {
        connect(rename_images_only_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked && offer_rename_images_checkbox && !offer_rename_images_checkbox->isChecked()) {
                QSignalBlocker blocker(offer_rename_images_checkbox);
                offer_rename_images_checkbox->setChecked(true);
            }
            settings.set_rename_images_only(checked);
            if (offer_rename_images_checkbox) {
                settings.set_offer_rename_images(offer_rename_images_checkbox->isChecked());
            }
            update_image_analysis_controls();
        });
    }

    if (image_options_toggle_button) {
        connect(image_options_toggle_button, &QToolButton::toggled, this, [this](bool) {
            update_image_analysis_controls();
        });
    }

    if (analyze_documents_checkbox) {
        connect(analyze_documents_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            settings.set_analyze_documents_by_content(checked);
            update_document_analysis_controls();
        });
    }

    if (process_documents_only_checkbox) {
        connect(process_documents_only_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            settings.set_process_documents_only(checked);
            update_document_analysis_controls();
        });
    }

    if (offer_rename_documents_checkbox) {
        connect(offer_rename_documents_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (!checked && rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked()) {
                QSignalBlocker blocker(rename_documents_only_checkbox);
                rename_documents_only_checkbox->setChecked(false);
            }
            settings.set_offer_rename_documents(checked);
            if (rename_documents_only_checkbox) {
                settings.set_rename_documents_only(rename_documents_only_checkbox->isChecked());
            }
            update_document_analysis_controls();
        });
    }

    if (rename_documents_only_checkbox) {
        connect(rename_documents_only_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked && offer_rename_documents_checkbox && !offer_rename_documents_checkbox->isChecked()) {
                QSignalBlocker blocker(offer_rename_documents_checkbox);
                offer_rename_documents_checkbox->setChecked(true);
            }
            settings.set_rename_documents_only(checked);
            if (offer_rename_documents_checkbox) {
                settings.set_offer_rename_documents(offer_rename_documents_checkbox->isChecked());
            }
            update_document_analysis_controls();
        });
    }

    if (add_document_date_to_category_checkbox) {
        connect(add_document_date_to_category_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            settings.set_add_document_date_to_category(checked);
        });
    }

    if (document_options_toggle_button) {
        connect(document_options_toggle_button, &QToolButton::toggled, this, [this](bool) {
            update_document_analysis_controls();
        });
    }
}

void MainApp::connect_whitelist_signals()
{
    connect(use_whitelist_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        if (whitelist_selector) {
            whitelist_selector->setEnabled(checked);
        }
        settings.set_use_whitelist(checked);
        apply_whitelist_to_selector();
    });

    connect(whitelist_selector, &QComboBox::currentTextChanged, this, [this](const QString& name) {
        settings.set_active_whitelist(name.toStdString());
        if (auto entry = whitelist_store.get(name.toStdString())) {
            settings.set_allowed_categories(entry->categories);
            settings.set_allowed_subcategories(entry->subcategories);
        }
    });
}


void MainApp::connect_edit_actions()
{
    path_entry->setContextMenuPolicy(Qt::DefaultContextMenu);
}


void MainApp::start_updater()
{
    auto* updater = new Updater(settings);
    updater->begin();
}


void MainApp::set_app_icon()
{
    const QString icon_path = QStringLiteral(":/net/quicknode/AIFileSorter/images/app_icon_128.png");
    QIcon icon(icon_path);
    if (icon.isNull()) {
        icon = QIcon(QStringLiteral(":/net/quicknode/AIFileSorter/images/logo.png"));
    }
    if (!icon.isNull()) {
        QApplication::setWindowIcon(icon);
        setWindowIcon(icon);
    }
}


void MainApp::load_settings()
{
    if (!settings.load()) {
        core_logger->info("Failed to load settings, using defaults.");
    }
    if (development_mode_) {
        development_prompt_logging_enabled_ = settings.get_development_prompt_logging();
    } else {
        development_prompt_logging_enabled_ = false;
    }
    apply_development_logging();
    TranslationManager::instance().set_language(settings.get_language());
    sync_settings_to_ui();
    retranslate_ui();
}


void MainApp::save_settings()
{
    sync_ui_to_settings();
    settings.save();
}


void MainApp::sync_settings_to_ui()
{
    restore_tree_settings();
    restore_sort_folder_state();
    restore_file_scan_options();
    restore_file_explorer_visibility();
    restore_development_preferences();

    if (ui_translator_) {
        ui_translator_->update_language_checks();
    }
}

void MainApp::restore_tree_settings()
{
    use_subcategories_checkbox->setChecked(settings.get_use_subcategories());
    set_categorization_style(settings.get_use_consistency_hints());
    if (use_whitelist_checkbox) {
        use_whitelist_checkbox->setChecked(settings.get_use_whitelist());
    }
    if (whitelist_selector) {
        apply_whitelist_to_selector();
    }
    categorize_files_checkbox->setChecked(settings.get_categorize_files());
    categorize_directories_checkbox->setChecked(settings.get_categorize_directories());
    if (analyze_images_checkbox) {
        QSignalBlocker blocker(analyze_images_checkbox);
        analyze_images_checkbox->setChecked(settings.get_analyze_images_by_content());
    }
    if (process_images_only_checkbox) {
        QSignalBlocker blocker(process_images_only_checkbox);
        process_images_only_checkbox->setChecked(settings.get_process_images_only());
    }
    if (offer_rename_images_checkbox) {
        QSignalBlocker blocker(offer_rename_images_checkbox);
        offer_rename_images_checkbox->setChecked(settings.get_offer_rename_images());
    }
    if (rename_images_only_checkbox) {
        QSignalBlocker blocker(rename_images_only_checkbox);
        rename_images_only_checkbox->setChecked(settings.get_rename_images_only());
    }
    if (image_options_toggle_button) {
        const bool expand_images = settings.get_process_images_only() ||
                                   settings.get_offer_rename_images() ||
                                   settings.get_rename_images_only();
        QSignalBlocker blocker(image_options_toggle_button);
        image_options_toggle_button->setChecked(expand_images);
    }
    if (analyze_documents_checkbox) {
        QSignalBlocker blocker(analyze_documents_checkbox);
        analyze_documents_checkbox->setChecked(settings.get_analyze_documents_by_content());
    }
    if (process_documents_only_checkbox) {
        QSignalBlocker blocker(process_documents_only_checkbox);
        process_documents_only_checkbox->setChecked(settings.get_process_documents_only());
    }
    if (offer_rename_documents_checkbox) {
        QSignalBlocker blocker(offer_rename_documents_checkbox);
        offer_rename_documents_checkbox->setChecked(settings.get_offer_rename_documents());
    }
    if (rename_documents_only_checkbox) {
        QSignalBlocker blocker(rename_documents_only_checkbox);
        rename_documents_only_checkbox->setChecked(settings.get_rename_documents_only());
    }
    if (add_document_date_to_category_checkbox) {
        QSignalBlocker blocker(add_document_date_to_category_checkbox);
        add_document_date_to_category_checkbox->setChecked(settings.get_add_document_date_to_category());
    }
    if (document_options_toggle_button) {
        const bool expand_documents = settings.get_process_documents_only() ||
                                      settings.get_offer_rename_documents() ||
                                      settings.get_rename_documents_only() ||
                                      settings.get_add_document_date_to_category();
        QSignalBlocker blocker(document_options_toggle_button);
        document_options_toggle_button->setChecked(expand_documents);
    }
    update_image_analysis_controls();
    update_document_analysis_controls();
}

void MainApp::restore_sort_folder_state()
{
    const QString stored_folder = QString::fromStdString(settings.get_sort_folder());
    QString effective_folder = stored_folder;

    if (effective_folder.isEmpty() || !QDir(effective_folder).exists()) {
        effective_folder = QDir::homePath();
    }

    path_entry->setText(effective_folder);

    if (!effective_folder.isEmpty() && QDir(effective_folder).exists()) {
        statusBar()->showMessage(tr("Loaded folder %1").arg(effective_folder), 3000);
        status_is_ready_ = false;
        update_folder_contents(effective_folder);
        focus_file_explorer_on_path(effective_folder);
    } else if (!stored_folder.isEmpty()) {
        core_logger->warn("Sort folder path is invalid: {}", stored_folder.toStdString());
    }
}

void MainApp::restore_file_scan_options()
{
    file_scan_options = FileScanOptions::None;
    if (settings.get_categorize_files()) {
        file_scan_options = file_scan_options | FileScanOptions::Files;
    }
    if (settings.get_categorize_directories()) {
        file_scan_options = file_scan_options | FileScanOptions::Directories;
    }
}

void MainApp::restore_file_explorer_visibility()
{
    const bool show_explorer = settings.get_show_file_explorer();
    if (file_explorer_dock) {
        file_explorer_dock->setVisible(show_explorer);
    }
    if (file_explorer_menu_action) {
        file_explorer_menu_action->setChecked(show_explorer);
    }
    update_results_view_mode();
}

void MainApp::restore_development_preferences()
{
    if (!development_mode_ || !development_prompt_logging_action) {
        return;
    }

    QSignalBlocker blocker(development_prompt_logging_action);
    development_prompt_logging_action->setChecked(development_prompt_logging_enabled_);
}


void MainApp::sync_ui_to_settings()
{
    settings.set_use_subcategories(use_subcategories_checkbox->isChecked());
    if (categorization_style_consistent_radio) {
        settings.set_use_consistency_hints(categorization_style_consistent_radio->isChecked());
    }
    if (use_whitelist_checkbox) {
        settings.set_use_whitelist(use_whitelist_checkbox->isChecked());
    }
    if (whitelist_selector) {
        settings.set_active_whitelist(whitelist_selector->currentText().toStdString());
    }
    settings.set_categorize_files(categorize_files_checkbox->isChecked());
    settings.set_categorize_directories(categorize_directories_checkbox->isChecked());
    if (analyze_images_checkbox) {
        settings.set_analyze_images_by_content(analyze_images_checkbox->isChecked());
    }
    if (process_images_only_checkbox) {
        settings.set_process_images_only(process_images_only_checkbox->isChecked());
    }
    if (offer_rename_images_checkbox) {
        settings.set_offer_rename_images(offer_rename_images_checkbox->isChecked());
    }
    if (rename_images_only_checkbox) {
        settings.set_rename_images_only(rename_images_only_checkbox->isChecked());
    }
    if (analyze_documents_checkbox) {
        settings.set_analyze_documents_by_content(analyze_documents_checkbox->isChecked());
    }
    if (process_documents_only_checkbox) {
        settings.set_process_documents_only(process_documents_only_checkbox->isChecked());
    }
    if (offer_rename_documents_checkbox) {
        settings.set_offer_rename_documents(offer_rename_documents_checkbox->isChecked());
    }
    if (rename_documents_only_checkbox) {
        settings.set_rename_documents_only(rename_documents_only_checkbox->isChecked());
    }
    if (add_document_date_to_category_checkbox) {
        settings.set_add_document_date_to_category(add_document_date_to_category_checkbox->isChecked());
    }
    const QByteArray folder_bytes = path_entry->text().toUtf8();
    settings.set_sort_folder(std::string(folder_bytes.constData(), static_cast<std::size_t>(folder_bytes.size())));
    if (file_explorer_menu_action) {
        settings.set_show_file_explorer(file_explorer_menu_action->isChecked());
    }
    if (consistency_pass_action) {
        settings.set_consistency_pass_enabled(consistency_pass_action->isChecked());
    }
    if (development_mode_ && development_prompt_logging_action) {
        const bool checked = development_prompt_logging_action->isChecked();
        development_prompt_logging_enabled_ = checked;
        settings.set_development_prompt_logging(checked);
        apply_development_logging();
    }
    if (language_group) {
        if (QAction* checked = language_group->checkedAction()) {
            settings.set_language(static_cast<Language>(checked->data().toInt()));
        }
    }
}

void MainApp::retranslate_ui()
{
    if (!ui_translator_) {
        return;
    }

    UiTranslator::State state{
        .analysis_in_progress = analysis_in_progress_,
        .stop_analysis_requested = stop_analysis.load(),
        .status_is_ready = status_is_ready_
    };
    ui_translator_->retranslate_all(state);
}

#if defined(AI_FILE_SORTER_TEST_BUILD)

QString MainAppTestAccess::analyze_button_text(const MainApp& app) {
    return app.analyze_button ? app.analyze_button->text() : QString();
}

QString MainAppTestAccess::path_label_text(const MainApp& app) {
    return app.path_label ? app.path_label->text() : QString();
}

QCheckBox* MainAppTestAccess::analyze_images_checkbox(MainApp& app) {
    return app.analyze_images_checkbox;
}

QCheckBox* MainAppTestAccess::process_images_only_checkbox(MainApp& app) {
    return app.process_images_only_checkbox;
}

QCheckBox* MainAppTestAccess::offer_rename_images_checkbox(MainApp& app) {
    return app.offer_rename_images_checkbox;
}

QCheckBox* MainAppTestAccess::rename_images_only_checkbox(MainApp& app) {
    return app.rename_images_only_checkbox;
}

void MainAppTestAccess::split_entries_for_analysis(const std::vector<FileEntry>& files,
                                                   bool analyze_images,
                                                   bool analyze_documents,
                                                   bool process_images_only,
                                                   bool process_documents_only,
                                                   bool rename_images_only,
                                                   bool rename_documents_only,
                                                   const std::unordered_set<std::string>& renamed_files,
                                                   std::vector<FileEntry>& image_entries,
                                                   std::vector<FileEntry>& document_entries,
                                                   std::vector<FileEntry>& other_entries) {
    ::split_entries_for_analysis(files,
                                 analyze_images,
                                 analyze_documents,
                                 process_images_only,
                                 process_documents_only,
                                 rename_images_only,
                                 rename_documents_only,
                                 renamed_files,
                                 image_entries,
                                 document_entries,
                                 other_entries);
}

void MainAppTestAccess::set_visual_llm_available_probe(MainApp& app, std::function<bool()> probe) {
    app.visual_llm_available_probe_ = std::move(probe);
}

void MainAppTestAccess::set_llm_selection_runner(MainApp& app, std::function<void()> runner) {
    app.llm_selection_runner_override_ = std::move(runner);
}

void MainAppTestAccess::set_image_analysis_prompt_override(MainApp& app, std::function<bool()> prompt) {
    app.image_analysis_prompt_override_ = std::move(prompt);
}

void MainAppTestAccess::trigger_retranslate(MainApp& app) {
    app.retranslate_ui();
}

void MainAppTestAccess::add_categorized_files(MainApp& app, int count) {
    app.record_categorized_metrics(count);
}

void MainAppTestAccess::simulate_support_prompt(Settings& settings,
                                                bool& prompt_state,
                                                int count,
                                                std::function<SimulatedSupportResult(int)> callback) {
    auto convert = [cb = std::move(callback)](int total) -> MainApp::SupportPromptResult {
        if (!cb) {
            return MainApp::SupportPromptResult::NotSure;
        }
        switch (cb(total)) {
            case SimulatedSupportResult::Support:
                return MainApp::SupportPromptResult::Support;
            case SimulatedSupportResult::CannotDonate:
                return MainApp::SupportPromptResult::CannotDonate;
            case SimulatedSupportResult::NotSure:
            default:
                return MainApp::SupportPromptResult::NotSure;
        }
    };

    record_categorized_metrics_impl(settings, prompt_state, count, convert);
}

#endif // AI_FILE_SORTER_TEST_BUILD

void MainApp::on_language_selected(Language language)
{
    settings.set_language(language);
    TranslationManager::instance().set_language(language);
    if (ui_translator_) {
        ui_translator_->update_language_checks();
    }
    retranslate_ui();

    if (categorization_dialog) {
        QCoreApplication::postEvent(
            categorization_dialog.get(),
            new QEvent(QEvent::LanguageChange));
    }
    if (progress_dialog) {
        QCoreApplication::postEvent(
            progress_dialog.get(),
            new QEvent(QEvent::LanguageChange));
    }
}

void MainApp::on_category_language_selected(CategoryLanguage language)
{
    settings.set_category_language(language);
    if (ui_translator_) {
        ui_translator_->update_language_checks();
    }
}


void MainApp::on_analyze_clicked()
{
    if (analyze_thread.joinable()) {
        stop_running_analysis();
        update_analyze_button_state(false);
        statusBar()->showMessage(tr("Analysis cancelled"), 4000);
        status_is_ready_ = false;
        return;
    }

    const std::string folder_path = get_folder_path();
    if (!Utils::is_valid_directory(folder_path.c_str())) {
        show_error_dialog(ERR_INVALID_PATH);
        core_logger->warn("User supplied invalid directory '{}'", folder_path);
        return;
    }

    if (!using_local_llm) {
        if (!Utils::is_network_available()) {
            show_error_dialog(ERR_NO_INTERNET_CONNECTION);
            core_logger->warn("Network unavailable when attempting to analyze '{}'", folder_path);
            return;
        }
        std::string credential_error;
        if (!categorization_service.ensure_remote_credentials(&credential_error)) {
            show_error_dialog(credential_error.empty()
                                  ? "Remote model credentials are missing or invalid. Please configure your API key and try again."
                                  : credential_error);
            return;
        }
    }

    if (!ensure_folder_categorization_style(folder_path)) {
        return;
    }

    stop_analysis = false;
    update_analyze_button_state(true);

    const bool show_subcategory = use_subcategories_checkbox->isChecked();
    progress_dialog = std::make_unique<CategorizationProgressDialog>(this, this, show_subcategory);
    progress_dialog->show();

    analyze_thread = std::thread([this]() {
        try {
            perform_analysis();
        } catch (const std::exception& ex) {
            core_logger->error("Exception during analysis: {}", ex.what());
            run_on_ui([this, message = std::string("Analysis error: ") + ex.what()]() {
                handle_analysis_failure(message);
            });
        }
    });
}


void MainApp::on_directory_selected(const QString& path, bool user_initiated)
{
    path_entry->setText(path);
    statusBar()->showMessage(tr("Folder selected: %1").arg(path), 3000);
    status_is_ready_ = false;

    if (!user_initiated) {
        focus_file_explorer_on_path(path);
    }

    update_folder_contents(path);
}

void MainApp::set_categorization_style(bool use_consistency)
{
    if (!categorization_style_refined_radio || !categorization_style_consistent_radio) {
        return;
    }

    QSignalBlocker blocker_refined(categorization_style_refined_radio);
    QSignalBlocker blocker_consistent(categorization_style_consistent_radio);
    categorization_style_refined_radio->setChecked(!use_consistency);
    categorization_style_consistent_radio->setChecked(use_consistency);
}

void MainApp::apply_whitelist_to_selector()
{
    if (!whitelist_selector) {
        return;
    }
    auto names = whitelist_store.list_names();
    if (names.empty()) {
        whitelist_store.ensure_default_from_legacy(settings.get_allowed_categories(),
                                                   settings.get_allowed_subcategories());
        whitelist_store.save();
        names = whitelist_store.list_names();
    }
    const QString current_active = QString::fromStdString(settings.get_active_whitelist());
    whitelist_selector->blockSignals(true);
    whitelist_selector->clear();
    for (const auto& name : names) {
        whitelist_selector->addItem(QString::fromStdString(name));
    }
    whitelist_selector->setEnabled(use_whitelist_checkbox && use_whitelist_checkbox->isChecked());
    int idx = whitelist_selector->findText(current_active);
    if (idx < 0 && !names.empty()) {
        const QString def = QString::fromStdString(whitelist_store.default_name());
        idx = whitelist_selector->findText(def);
        if (idx < 0) {
            idx = 0;
        }
    }
    if (idx >= 0) {
        whitelist_selector->setCurrentIndex(idx);
        const QString chosen = whitelist_selector->itemText(idx);
        settings.set_active_whitelist(chosen.toStdString());
        if (auto entry = whitelist_store.get(chosen.toStdString())) {
            settings.set_allowed_categories(entry->categories);
            settings.set_allowed_subcategories(entry->subcategories);
        }
    }
    whitelist_selector->blockSignals(false);
}

void MainApp::show_whitelist_manager()
{
    if (!whitelist_dialog) {
        whitelist_dialog = std::make_unique<WhitelistManagerDialog>(whitelist_store, this);
        whitelist_dialog->set_on_lists_changed([this]() {
            whitelist_store.load();
            whitelist_store.save();
            apply_whitelist_to_selector();
        });
    }
    whitelist_dialog->show();
    whitelist_dialog->raise();
    whitelist_dialog->activateWindow();
}

void MainApp::initialize_whitelists()
{
    whitelist_store.initialize_from_settings(settings);
}

bool MainApp::ensure_folder_categorization_style(const std::string& folder_path)
{
    const auto cached_style = db_manager.get_directory_categorization_style(folder_path);
    if (!cached_style.has_value()) {
        return true;
    }

    const bool desired = settings.get_use_consistency_hints();
    if (cached_style.value() == desired) {
        return true;
    }

    const auto style_label = [](bool value) -> QString {
        return value ? tr("More consistent") : tr("More refined");
    };

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Recategorize folder?"));
    box.setText(tr("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?")
                    .arg(style_label(cached_style.value()), style_label(desired)));
    QPushButton* recategorize_button = box.addButton(tr("Recategorize"), QMessageBox::AcceptRole);
    box.addButton(tr("Keep existing"), QMessageBox::RejectRole);
    QPushButton* cancel_button = box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == cancel_button) {
        return false;
    }

    if (box.clickedButton() == recategorize_button) {
        if (!db_manager.clear_directory_categorizations(folder_path)) {
            show_error_dialog(tr("Failed to reset cached categorization for this folder.").toStdString());
            return false;
        }
    }

    return true;
}


void MainApp::ensure_one_checkbox_active(QCheckBox* changed_checkbox)
{
    if (!categorize_files_checkbox || !categorize_directories_checkbox) {
        return;
    }

    if (!categorize_files_checkbox->isChecked() && !categorize_directories_checkbox->isChecked()) {
        QCheckBox* other = (changed_checkbox == categorize_files_checkbox)
                               ? categorize_directories_checkbox
                               : categorize_files_checkbox;
        other->setChecked(true);
    }
}


void MainApp::update_file_scan_option(FileScanOptions option, bool enabled)
{
    if (enabled) {
        file_scan_options = file_scan_options | option;
    } else {
        file_scan_options = file_scan_options & ~option;
    }
}

FileScanOptions MainApp::effective_scan_options() const
{
    const bool images_only = settings.get_process_images_only() && settings.get_analyze_images_by_content();
    const bool documents_only = settings.get_process_documents_only() && settings.get_analyze_documents_by_content();
    if (images_only || documents_only) {
        return FileScanOptions::Files;
    }
    return file_scan_options;
}

bool MainApp::visual_llm_files_available() const
{
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (visual_llm_available_probe_) {
        return visual_llm_available_probe_();
    }
#endif
    const char* model_url = std::getenv("LLAVA_MODEL_URL");
    const char* mmproj_url = std::getenv("LLAVA_MMPROJ_URL");
    if (!model_url || *model_url == '\0' || !mmproj_url || *mmproj_url == '\0') {
        return false;
    }

    const auto model_path = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(model_url));
    const auto mmproj_path = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(mmproj_url));

    if (!std::filesystem::exists(model_path)) {
        return false;
    }

    if (std::filesystem::exists(mmproj_path)) {
        return true;
    }

    const auto llm_dir = std::filesystem::path(Utils::get_default_llm_destination());
    static const char* kAltMmprojNames[] = {
        "mmproj-model-f16.gguf",
        "llava-v1.6-mistral-7b-mmproj-f16.gguf"
    };
    for (const char* alt_name : kAltMmprojNames) {
        if (std::filesystem::exists(llm_dir / alt_name)) {
            return true;
        }
    }

    return false;
}

void MainApp::update_image_analysis_controls()
{
    if (!analyze_images_checkbox ||
        !process_images_only_checkbox ||
        !offer_rename_images_checkbox ||
        !rename_images_only_checkbox) {
        return;
    }

    const bool analysis_enabled = analyze_images_checkbox->isChecked();
    process_images_only_checkbox->setEnabled(analysis_enabled);
    offer_rename_images_checkbox->setEnabled(analysis_enabled);
    rename_images_only_checkbox->setEnabled(analysis_enabled);
    if (image_options_toggle_button) {
        image_options_toggle_button->setEnabled(analysis_enabled);
        const bool expanded = image_options_toggle_button->isChecked();
        image_options_toggle_button->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        if (image_options_container) {
            image_options_container->setVisible(analysis_enabled && expanded);
        }
    } else if (image_options_container) {
        image_options_container->setVisible(analysis_enabled);
    }

    if (analysis_enabled &&
        rename_images_only_checkbox->isChecked() &&
        !offer_rename_images_checkbox->isChecked()) {
        QSignalBlocker blocker(offer_rename_images_checkbox);
        offer_rename_images_checkbox->setChecked(true);
    }

    update_image_only_controls();
}

void MainApp::update_image_only_controls()
{
    if (!process_images_only_checkbox && !process_documents_only_checkbox) {
        return;
    }

    const bool analyze_images = analyze_images_checkbox && analyze_images_checkbox->isChecked();
    const bool analyze_documents = analyze_documents_checkbox && analyze_documents_checkbox->isChecked();
    const bool images_only_active = process_images_only_checkbox &&
                                    analyze_images &&
                                    process_images_only_checkbox->isChecked();
    const bool documents_only_active = process_documents_only_checkbox &&
                                       analyze_documents &&
                                       process_documents_only_checkbox->isChecked();
    const bool rename_images_active = rename_images_only_checkbox &&
                                      analyze_images &&
                                      rename_images_only_checkbox->isChecked();
    const bool rename_documents_active = rename_documents_only_checkbox &&
                                         analyze_documents &&
                                         rename_documents_only_checkbox->isChecked();
    const bool disable_files_categorization = rename_images_active || rename_documents_active;
    const bool disable_directories_categorization = images_only_active || documents_only_active;

    if (use_subcategories_checkbox) {
        use_subcategories_checkbox->setEnabled(!disable_files_categorization);
    }
    if (categorize_files_checkbox) {
        categorize_files_checkbox->setEnabled(!disable_files_categorization);
    }
    if (categorize_directories_checkbox) {
        categorize_directories_checkbox->setEnabled(!disable_directories_categorization);
    }
    if (categorization_style_heading) {
        categorization_style_heading->setEnabled(!disable_files_categorization);
    }
    if (categorization_style_refined_radio) {
        categorization_style_refined_radio->setEnabled(!disable_files_categorization);
    }
    if (categorization_style_consistent_radio) {
        categorization_style_consistent_radio->setEnabled(!disable_files_categorization);
    }
    if (use_whitelist_checkbox) {
        use_whitelist_checkbox->setEnabled(!disable_files_categorization);
    }
    if (whitelist_selector) {
        const bool whitelist_enabled = !disable_files_categorization &&
                                       use_whitelist_checkbox &&
                                       use_whitelist_checkbox->isChecked();
        whitelist_selector->setEnabled(whitelist_enabled);
    }
}

void MainApp::update_document_analysis_controls()
{
    if (!analyze_documents_checkbox ||
        !process_documents_only_checkbox ||
        !offer_rename_documents_checkbox ||
        !rename_documents_only_checkbox ||
        !add_document_date_to_category_checkbox) {
        return;
    }

    const bool analysis_enabled = analyze_documents_checkbox->isChecked();
    process_documents_only_checkbox->setEnabled(analysis_enabled);
    offer_rename_documents_checkbox->setEnabled(analysis_enabled);
    rename_documents_only_checkbox->setEnabled(analysis_enabled);

    const bool rename_only = analysis_enabled && rename_documents_only_checkbox->isChecked();
    add_document_date_to_category_checkbox->setEnabled(analysis_enabled && !rename_only);

    if (analysis_enabled &&
        rename_documents_only_checkbox->isChecked() &&
        !offer_rename_documents_checkbox->isChecked()) {
        QSignalBlocker blocker(offer_rename_documents_checkbox);
        offer_rename_documents_checkbox->setChecked(true);
    }

    if (document_options_toggle_button) {
        document_options_toggle_button->setEnabled(analysis_enabled);
        const bool expanded = document_options_toggle_button->isChecked();
        document_options_toggle_button->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        if (document_options_container) {
            document_options_container->setVisible(analysis_enabled && expanded);
        }
    } else if (document_options_container) {
        document_options_container->setVisible(analysis_enabled);
    }

    update_image_only_controls();
}

void MainApp::run_llm_selection_dialog_for_visual()
{
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (llm_selection_runner_override_) {
        llm_selection_runner_override_();
        return;
    }
#endif
    show_llm_selection_dialog();
}

void MainApp::handle_image_analysis_toggle(bool checked)
{
    if (!analyze_images_checkbox) {
        return;
    }

    if (checked && !visual_llm_files_available()) {
        bool should_open_dialog = false;
#ifdef AI_FILE_SORTER_TEST_BUILD
        if (image_analysis_prompt_override_) {
            should_open_dialog = image_analysis_prompt_override_();
        } else
#endif
        {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Information);
            box.setWindowTitle(tr("Download required"));
            box.setText(tr("Image analysis requires visual LLM files. Download them now?"));
            QPushButton* ok_button = box.addButton(tr("OK"), QMessageBox::AcceptRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(ok_button);
            box.exec();
            should_open_dialog = (box.clickedButton() == ok_button);
        }

        if (!should_open_dialog) {
            QSignalBlocker blocker(analyze_images_checkbox);
            analyze_images_checkbox->setChecked(false);
            settings.set_analyze_images_by_content(false);
            update_image_analysis_controls();
            return;
        }

        run_llm_selection_dialog_for_visual();

        if (!visual_llm_files_available()) {
            QSignalBlocker blocker(analyze_images_checkbox);
            analyze_images_checkbox->setChecked(false);
            settings.set_analyze_images_by_content(false);
            update_image_analysis_controls();
            return;
        }
    }

    settings.set_analyze_images_by_content(analyze_images_checkbox->isChecked());
    update_image_analysis_controls();
}


void MainApp::update_analyze_button_state(bool analyzing)
{
    analysis_in_progress_ = analyzing;
    if (analyzing) {
        analyze_button->setText(tr("Stop analyzing"));
        statusBar()->showMessage(tr("Analyzing…"));
        status_is_ready_ = false;
    } else {
        analyze_button->setText(tr("Analyze folder"));
        statusBar()->showMessage(tr("Ready"));
        status_is_ready_ = true;
    }
}

void MainApp::update_results_view_mode()
{
    if (!results_stack) {
        return;
    }

    const bool explorer_visible = file_explorer_dock && file_explorer_dock->isVisible();
    const int target_index = explorer_visible ? folder_view_page_index_ : tree_view_page_index_;
    if (target_index >= 0 && target_index < results_stack->count()) {
        results_stack->setCurrentIndex(target_index);
    }

    if (explorer_visible && path_entry) {
        update_folder_contents(path_entry->text());
    }
}

void MainApp::update_folder_contents(const QString& directory)
{
    if (!folder_contents_model || !folder_contents_view || directory.isEmpty()) {
        return;
    }

    QDir dir(directory);
    if (!dir.exists()) {
        return;
    }

    const bool previous_flag = suppress_folder_view_sync_;
    suppress_folder_view_sync_ = true;

    const QModelIndex new_root = folder_contents_model->setRootPath(directory);
    folder_contents_view->setRootIndex(new_root);
    folder_contents_view->scrollTo(new_root, QAbstractItemView::PositionAtTop);

    folder_contents_view->resizeColumnToContents(0);

    suppress_folder_view_sync_ = previous_flag;
}

void MainApp::focus_file_explorer_on_path(const QString& path)
{
    if (!file_system_model || !file_explorer_view || path.isEmpty()) {
        return;
    }

    const QModelIndex index = file_system_model->index(path);
    if (!index.isValid()) {
        return;
    }

    const bool previous_suppress = suppress_explorer_sync_;
    suppress_explorer_sync_ = true;

    file_explorer_view->setCurrentIndex(index);
    file_explorer_view->expand(index);
    file_explorer_view->scrollTo(index, QAbstractItemView::PositionAtCenter);

    suppress_explorer_sync_ = previous_suppress;
}

void MainApp::record_categorized_metrics(int count)
{
    record_categorized_metrics_impl(
        settings,
        donation_prompt_active_,
        count,
        [this](int total) { return show_support_prompt_dialog(total); });
}

void MainApp::undo_last_run()
{
    const auto latest = undo_manager_.latest_plan_path();
    if (!latest) {
        show_error_dialog("No undo plans available.");
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Undo last run"));
    box.setText(tr("This will attempt to move files back to their original locations based on the last run.\n\nPlan file: %1")
                    .arg(*latest));
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Ok) {
        return;
    }

    const auto res = undo_manager_.undo_plan(*latest);
    QString summary = tr("Restored %1 file(s). Skipped %2.").arg(res.restored).arg(res.skipped);
    if (!res.details.isEmpty()) {
        summary.append("\n");
        summary.append(res.details.join("\n"));
    }

    QMessageBox::information(this, tr("Undo complete"), summary);
    if (ui_logger) {
        ui_logger->info(summary.toStdString());
    }
    if (res.restored > 0) {
        QFile::remove(*latest);
    }
}

bool MainApp::perform_undo_from_plan(const QString& plan_path)
{
    const auto res = undo_manager_.undo_plan(plan_path);
    QString summary = tr("Restored %1 file(s). Skipped %2.").arg(res.restored).arg(res.skipped);
    if (!res.details.isEmpty()) {
        summary.append("\n");
        summary.append(res.details.join("\n"));
    }
    QMessageBox::information(this, tr("Undo complete"), summary);
    return res.restored > 0;
}

MainApp::SupportPromptResult MainApp::show_support_prompt_dialog(int total_files)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Support AI File Sorter"));

    const QString headline = tr("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you.")
                                 .arg(total_files);
    const QString details = tr("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                               "If the app saves you time or brings value, please consider supporting it so it can keep improving.");

    box.setText(headline);
    box.setInformativeText(details);

    auto* support_btn = box.addButton(tr("Support"), QMessageBox::ActionRole);
    auto* later_btn = box.addButton(tr("I'm not yet sure"), QMessageBox::ActionRole);
    auto* cannot_btn = box.addButton(tr("I cannot donate"), QMessageBox::ActionRole);

    const auto apply_button_style = [](QAbstractButton* button,
                                       const QString& background,
                                       const QString& hover) {
        if (!button) {
            return;
        }
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  color: white;"
            "  padding: 6px 18px;"
            "  border: none;"
            "  border-radius: 14px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "  background-color: %2;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %2;"
            "  opacity: 0.9;"
            "}"
        ).arg(background, hover));
    };

    apply_button_style(support_btn, QStringLiteral("#007aff"), QStringLiteral("#005ec7"));
    const QString neutral_bg = QStringLiteral("#bdc3c7");
    const QString neutral_hover = QStringLiteral("#95a5a6");
    apply_button_style(later_btn, neutral_bg, neutral_hover);
    apply_button_style(cannot_btn, neutral_bg, neutral_hover);

    if (auto* button_box = box.findChild<QDialogButtonBox*>()) {
        button_box->setCenterButtons(true);
        // Ensure the visual order matches creation (Support, Not sure, Cannot donate)
        button_box->setLayoutDirection(Qt::LeftToRight);
    }

    box.setDefaultButton(later_btn);
    box.exec();

    const QAbstractButton* clicked = box.clickedButton();
    if (clicked == support_btn) {
        MainAppHelpActions::open_support_page();
        return SupportPromptResult::Support;
    }
    if (clicked == cannot_btn) {
        return SupportPromptResult::CannotDonate;
    }
    return SupportPromptResult::NotSure;
}


void MainApp::handle_analysis_finished()
{
    update_analyze_button_state(false);

    if (analyze_thread.joinable()) {
        analyze_thread.join();
    }

    if (progress_dialog) {
        progress_dialog->hide();
        progress_dialog.reset();
    }

    stop_analysis = false;

    if (new_files_to_sort.empty()) {
        handle_no_files_to_sort();
        return;
    }

    populate_tree_view(new_files_to_sort);
    show_results_dialog(new_files_to_sort);
}

void MainApp::handle_analysis_cancelled()
{
    update_analyze_button_state(false);

    if (analyze_thread.joinable()) {
        analyze_thread.join();
    }

    if (progress_dialog) {
        progress_dialog->hide();
        progress_dialog.reset();
    }

    stop_analysis = false;
    statusBar()->showMessage(tr("Analysis cancelled"), 4000);
}


void MainApp::handle_analysis_failure(const std::string& message)
{
    update_analyze_button_state(false);
    if (analyze_thread.joinable()) {
        analyze_thread.join();
    }
    if (progress_dialog) {
        progress_dialog->hide();
        progress_dialog.reset();
    }
    stop_analysis = false;
    show_error_dialog(message);
}


void MainApp::handle_no_files_to_sort()
{
    show_error_dialog(ERR_NO_FILES_TO_CATEGORIZE);
}


void MainApp::populate_tree_view(const std::vector<CategorizedFile>& files)
{
    tree_model->removeRows(0, tree_model->rowCount());

    for (const auto& file : files) {
        QList<QStandardItem*> row;
        auto* file_item = new QStandardItem(QString::fromStdString(file.file_name));
        auto* type_item = new QStandardItem(file.type == FileType::Directory ? tr("Directory") : tr("File"));
        type_item->setData(file.type == FileType::Directory ? QStringLiteral("D") : QStringLiteral("F"), Qt::UserRole);
        auto* category_item = new QStandardItem(QString::fromStdString(file.category));
        auto* subcategory_item = new QStandardItem(QString::fromStdString(file.subcategory));
        auto* status_item = new QStandardItem(tr("Ready"));
        status_item->setData(QStringLiteral("ready"), Qt::UserRole);
        row << file_item << type_item << category_item << subcategory_item << status_item;
        tree_model->appendRow(row);
    }
}



void MainApp::append_progress(const std::string& message)
{
    run_on_ui([this, message]() {
        if (progress_dialog) {
            progress_dialog->append_text(message);
        }
    });
}

bool MainApp::should_abort_analysis() const
{
    return stop_analysis.load();
}

void MainApp::prune_empty_cached_entries_for(const std::string& directory_path)
{
    const std::vector<CategorizedFile> cleared =
        categorization_service.prune_empty_cached_entries(directory_path);
    if (cleared.empty()) {
        return;
    }

    if (core_logger) {
        core_logger->warn("Cleared {} cached categorization entr{} with empty values for '{}'",
                          cleared.size(),
                          cleared.size() == 1 ? "y" : "ies",
                          directory_path);
        for (const auto& entry : cleared) {
            core_logger->warn("  - {}", entry.file_name);
        }
    }
    std::string reason = "Cached category was empty. The item will be analyzed again.";
    if (!using_local_llm) {
        reason += " Configure your remote API key before analyzing.";
    }
    notify_recategorization_reset(cleared, reason);
}

void MainApp::log_cached_highlights()
{
    if (already_categorized_files.empty()) {
        return;
    }
    append_progress(to_utf8(tr("[ARCHIVE] Already categorized highlights:")));
    for (const auto& file_entry : already_categorized_files) {
        const QString type_label = file_entry.type == FileType::Directory ? tr("Directory") : tr("File");
        const QString sub = file_entry.subcategory.empty()
            ? QStringLiteral("-")
            : QString::fromStdString(file_entry.subcategory);
        append_progress(to_utf8(QStringLiteral("  - [%1] %2 -> %3 / %4")
                                    .arg(type_label,
                                         QString::fromStdString(file_entry.file_name),
                                         QString::fromStdString(file_entry.category),
                                         sub)));
    }
}

void MainApp::log_pending_queue()
{
    if (!progress_dialog) {
        return;
    }
    if (files_to_categorize.empty()) {
        append_progress(to_utf8(tr("[DONE] No files to categorize.")));
        return;
    }

    append_progress(to_utf8(tr("[QUEUE] Items waiting for categorization:")));
    for (const auto& file_entry : files_to_categorize) {
        const QString type_label = file_entry.type == FileType::Directory ? tr("Directory") : tr("File");
        append_progress(to_utf8(QStringLiteral("  - [%1] %2")
                                    .arg(type_label, QString::fromStdString(file_entry.file_name))));
    }
}

void MainApp::perform_analysis()
{
    const std::string directory_path = get_folder_path();
    core_logger->info("Starting analysis for directory '{}'", directory_path);

    bool stop_requested = false;
    auto update_stop = [this, &stop_requested]() {
        if (!stop_requested && should_abort_analysis()) {
            stop_requested = true;
        }
        return stop_requested;
    };

    append_progress(to_utf8(tr("[SCAN] Exploring %1")
                                .arg(QString::fromStdString(directory_path))));
    update_stop();

    try {
        prune_empty_cached_entries_for(directory_path);
        const bool analyze_images = settings.get_analyze_images_by_content();
        const bool analyze_documents = settings.get_analyze_documents_by_content();
        const bool process_images_only = analyze_images && settings.get_process_images_only();
        const bool process_documents_only = analyze_documents && settings.get_process_documents_only();
        const bool rename_images_only = analyze_images && settings.get_rename_images_only();
        const bool rename_documents_only = analyze_documents && settings.get_rename_documents_only();
        const bool offer_image_renames = analyze_images && settings.get_offer_rename_images();
        const bool offer_document_renames = analyze_documents && settings.get_offer_rename_documents();
        const bool wants_visual_rename = analyze_images && offer_image_renames && !rename_images_only;
        const bool wants_document_rename = analyze_documents && offer_document_renames && !rename_documents_only;
        const bool add_document_date = analyze_documents && settings.get_add_document_date_to_category();

        const auto cached_entries = categorization_service.load_cached_entries(directory_path);
        std::vector<CategorizedFile> pending_renames;
        pending_renames.reserve(cached_entries.size());
        std::unordered_set<std::string> renamed_files;
        already_categorized_files.clear();
        already_categorized_files.reserve(cached_entries.size());
        std::vector<FileEntry> cached_image_entries_for_visual;
        std::unordered_map<std::string, size_t> cached_visual_indices;
        std::vector<FileEntry> cached_document_entries_for_analysis;
        std::unordered_map<std::string, size_t> cached_document_indices;
        std::unordered_map<std::string, std::string> cached_image_suggestions;
        std::unordered_map<std::string, std::string> cached_document_suggestions;
        if (wants_visual_rename) {
            cached_image_entries_for_visual.reserve(cached_entries.size());
            cached_visual_indices.reserve(cached_entries.size());
        }
        if (wants_document_rename) {
            cached_document_entries_for_analysis.reserve(cached_entries.size());
            cached_document_indices.reserve(cached_entries.size());
        }

        auto to_lower = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        };
        auto has_category = [](const CategorizedFile& entry) {
            return !entry.category.empty() && !entry.subcategory.empty();
        };
        auto is_supported_image_entry = [](const CategorizedFile& entry) {
            if (entry.type != FileType::File) {
                return false;
            }
            const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                   Utils::utf8_to_path(entry.file_name);
            return LlavaImageAnalyzer::is_supported_image(full_path);
        };
        auto is_supported_document_entry = [](const CategorizedFile& entry) {
            if (entry.type != FileType::File) {
                return false;
            }
            const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                   Utils::utf8_to_path(entry.file_name);
            return DocumentTextAnalyzer::is_supported_document(full_path);
        };
        auto is_missing_category_label = [](const std::string& value) {
            std::string trimmed = value;
            const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
            trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
            if (trimmed.empty()) {
                return true;
            }
            std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return trimmed == "uncategorized";
        };
        auto persist_rename_only_progress = [this, &is_missing_category_label](const FileEntry& entry,
                                                                              const std::string& suggested_name) {
            // Persist rename-only progress during analysis to avoid losing rename suggestions on crash.
            const auto entry_path = Utils::utf8_to_path(entry.full_path);
            const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
            const auto cached_entry = db_manager.get_categorized_file(dir_path, entry.file_name, entry.type);

            std::string category;
            std::string subcategory;
            bool used_consistency = false;
            DatabaseManager::ResolvedCategory resolved{0, "", ""};

            if (cached_entry) {
                category = cached_entry->category;
                subcategory = cached_entry->subcategory;
                if (is_missing_category_label(category)) {
                    category.clear();
                }
                if (is_missing_category_label(subcategory)) {
                    subcategory.clear();
                }
                if (!category.empty()) {
                    resolved.category = category;
                    resolved.subcategory = subcategory;
                    resolved.taxonomy_id = cached_entry->taxonomy_id;
                }
                used_consistency = cached_entry->used_consistency_hints;
            }

            const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
            db_manager.insert_or_update_file_with_categorization(
                entry.file_name,
                file_type_label,
                dir_path,
                resolved,
                used_consistency,
                suggested_name,
                true);
        };
        auto persist_cached_suggestion = [this](const CategorizedFile& entry,
                                                const std::string& suggested_name) {
            DatabaseManager::ResolvedCategory resolved{entry.taxonomy_id, entry.category, entry.subcategory};
            const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
            db_manager.insert_or_update_file_with_categorization(
                entry.file_name,
                file_type_label,
                entry.file_path,
                resolved,
                entry.used_consistency_hints,
                suggested_name,
                entry.rename_only,
                entry.rename_applied);
        };

        for (const auto& entry : cached_entries) {
            const bool is_image_entry = is_supported_image_entry(entry);
            const bool is_document_entry = is_supported_document_entry(entry);
            const bool suggested_matches = !entry.suggested_name.empty() &&
                                           to_lower(entry.suggested_name) == to_lower(entry.file_name);
            const bool already_renamed = entry.rename_applied || suggested_matches;
            if (already_renamed) {
                renamed_files.insert(entry.file_name);
            }
            if (entry.rename_only && !has_category(entry)) {
                if (!already_renamed) {
                    pending_renames.push_back(entry);
                    if (is_image_entry && !rename_images_only && !entry.suggested_name.empty()) {
                        cached_image_suggestions.emplace(entry.file_name, entry.suggested_name);
                    }
                    if (is_document_entry && !rename_documents_only && !entry.suggested_name.empty()) {
                        cached_document_suggestions.emplace(entry.file_name, entry.suggested_name);
                    }
                }
                continue;
            }
            if (rename_images_only && analyze_images && is_image_entry) {
                if (!already_renamed && entry.suggested_name.empty()) {
                    continue;
                }
                CategorizedFile adjusted = entry;
                adjusted.rename_only = true;
                already_categorized_files.push_back(std::move(adjusted));
                continue;
            }
            if (rename_documents_only && analyze_documents && is_document_entry) {
                if (!already_renamed && entry.suggested_name.empty()) {
                    continue;
                }
                CategorizedFile adjusted = entry;
                adjusted.rename_only = true;
                already_categorized_files.push_back(std::move(adjusted));
                continue;
            }
            if (wants_visual_rename && is_image_entry && entry.suggested_name.empty() && !already_renamed) {
                const auto entry_index = already_categorized_files.size();
                already_categorized_files.push_back(entry);
                cached_visual_indices.emplace(entry.file_name, entry_index);
                const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                       Utils::utf8_to_path(entry.file_name);
                cached_image_entries_for_visual.push_back(
                    FileEntry{Utils::path_to_utf8(full_path), entry.file_name, entry.type});
                continue;
            }
            if (wants_document_rename && is_document_entry && entry.suggested_name.empty() && !already_renamed) {
                const auto entry_index = already_categorized_files.size();
                already_categorized_files.push_back(entry);
                cached_document_indices.emplace(entry.file_name, entry_index);
                const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                       Utils::utf8_to_path(entry.file_name);
                cached_document_entries_for_analysis.push_back(
                    FileEntry{Utils::path_to_utf8(full_path), entry.file_name, entry.type});
                continue;
            }
            already_categorized_files.push_back(entry);
        }

        if (process_images_only || process_documents_only) {
            const bool allow_images = process_images_only;
            const bool allow_documents = process_documents_only;
            auto filter_entries = [&](std::vector<CategorizedFile>& entries) {
                entries.erase(
                    std::remove_if(entries.begin(),
                                   entries.end(),
                                   [&](const CategorizedFile& entry) {
                                       if (entry.type != FileType::File) {
                                           return true;
                                       }
                                       const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                                              Utils::utf8_to_path(entry.file_name);
                                       const bool is_image = LlavaImageAnalyzer::is_supported_image(full_path);
                                       const bool is_document = DocumentTextAnalyzer::is_supported_document(full_path);
                                       if (is_image && allow_images) {
                                           return false;
                                       }
                                       if (is_document && allow_documents) {
                                           return false;
                                       }
                                       return true;
                                   }),
                    entries.end());
            };
            filter_entries(already_categorized_files);
            filter_entries(pending_renames);
            if (!cached_visual_indices.empty()) {
                for (size_t index = 0; index < already_categorized_files.size(); ++index) {
                    auto it = cached_visual_indices.find(already_categorized_files[index].file_name);
                    if (it != cached_visual_indices.end()) {
                        it->second = index;
                    }
                }
            }
            if (!cached_document_indices.empty()) {
                for (size_t index = 0; index < already_categorized_files.size(); ++index) {
                    auto it = cached_document_indices.find(already_categorized_files[index].file_name);
                    if (it != cached_document_indices.end()) {
                        it->second = index;
                    }
                }
            }
        }

        update_stop();

        log_cached_highlights();

        auto cached_file_names = results_coordinator.extract_file_names(already_categorized_files);
        if ((rename_images_only || rename_documents_only) && !pending_renames.empty()) {
            for (const auto& entry : pending_renames) {
                cached_file_names.insert(entry.file_name);
            }
        }
        const auto scan_options = effective_scan_options();
        files_to_categorize = results_coordinator.find_files_to_categorize(directory_path, scan_options, cached_file_names);
        if (process_images_only || process_documents_only) {
            const bool allow_images = process_images_only;
            const bool allow_documents = process_documents_only;
            files_to_categorize.erase(
                std::remove_if(files_to_categorize.begin(),
                               files_to_categorize.end(),
                               [&](const FileEntry& entry) {
                                   if (entry.type != FileType::File) {
                                       return true;
                                   }
                                   const bool is_image = LlavaImageAnalyzer::is_supported_image(entry.full_path);
                                   const bool is_document = DocumentTextAnalyzer::is_supported_document(entry.full_path);
                                   if (is_image && allow_images) {
                                       return false;
                                   }
                                   if (is_document && allow_documents) {
                                       return false;
                                   }
                                   return true;
                               }),
                files_to_categorize.end());
        }
        core_logger->debug("Found {} item(s) pending categorization in '{}'.",
                           files_to_categorize.size(), directory_path);

        log_pending_queue();
        update_stop();

        append_progress(to_utf8(tr("[PROCESS] Letting the AI do its magic...")));

        std::vector<FileEntry> image_entries;
        std::vector<FileEntry> document_entries;
        std::vector<FileEntry> other_entries;
        split_entries_for_analysis(files_to_categorize,
                                   analyze_images,
                                   analyze_documents,
                                   process_images_only,
                                   process_documents_only,
                                   rename_images_only,
                                   rename_documents_only,
                                   renamed_files,
                                   image_entries,
                                   document_entries,
                                   other_entries);
        if (!cached_image_entries_for_visual.empty()) {
            image_entries.insert(image_entries.end(),
                                 cached_image_entries_for_visual.begin(),
                                 cached_image_entries_for_visual.end());
        }
        if (!cached_document_entries_for_analysis.empty()) {
            document_entries.insert(document_entries.end(),
                                    cached_document_entries_for_analysis.begin(),
                                    cached_document_entries_for_analysis.end());
        }

        struct ImageAnalysisInfo {
            std::string suggested_name;
            std::string prompt_name;
            std::string prompt_path;
        };

        struct DocumentAnalysisInfo {
            std::string suggested_name;
            std::string prompt_name;
            std::string prompt_path;
        };

        std::unordered_map<std::string, ImageAnalysisInfo> image_info;
        std::vector<FileEntry> image_entries_for_llm;
        image_entries_for_llm.reserve(image_entries.size());
        std::vector<FileEntry> analyzed_image_entries;
        analyzed_image_entries.reserve(image_entries.size());

        std::unordered_map<std::string, DocumentAnalysisInfo> document_info;
        std::unordered_map<std::string, std::string> document_dates;
        std::vector<FileEntry> document_entries_for_llm;
        document_entries_for_llm.reserve(document_entries.size());
        std::vector<FileEntry> analyzed_document_entries;
        analyzed_document_entries.reserve(document_entries.size());

        if (analyze_images && !image_entries.empty()) {
            std::string error;
            auto visual_paths = resolve_visual_llm_paths(&error);
            if (!visual_paths) {
                throw std::runtime_error(error);
            }

            LlavaImageAnalyzer::Settings vision_settings;
            vision_settings.use_gpu = should_use_visual_gpu();
            const auto visual_gpu_override = read_env_bool("AI_FILE_SORTER_VISUAL_USE_GPU");
            if (visual_gpu_override.has_value()) {
                vision_settings.use_gpu = *visual_gpu_override;
            }
            vision_settings.batch_progress = [this](int current_batch, int total_batches) {
                if (total_batches <= 0 || current_batch <= 0) {
                    return;
                }
                const double percent = (static_cast<double>(current_batch) /
                                        static_cast<double>(total_batches)) * 100.0;
                append_progress(to_utf8(tr("[VISION] Decoding image batch %1/%2 (%3%)")
                                            .arg(current_batch)
                                            .arg(total_batches)
                                            .arg(percent, 0, 'f', 2)));
            };
            vision_settings.log_visual_output = should_log_prompts();

            const bool allow_visual_cpu_fallback = vision_settings.use_gpu && !visual_gpu_override.has_value();
            std::optional<bool> visual_cpu_fallback_choice;
            bool visual_cpu_fallback_active = false;

            auto should_retry_on_cpu = [](const std::exception& ex) {
                const std::string message = ex.what();
                return message.find("Failed to create llama_context") != std::string::npos;
            };

            auto prompt_visual_cpu_fallback = [this]() -> bool {
                auto show_dialog = [this]() -> bool {
                    QMessageBox box(this);
                    box.setIcon(QMessageBox::Question);
                    box.setWindowTitle(tr("Switch image analysis to CPU?"));
                    box.setText(tr("Image analysis ran out of GPU memory."));
                    box.setInformativeText(tr("Retry on CPU instead? Cancel will skip visual analysis and fall back to "
                                              "filename-based categorization."));
                    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
                    box.setDefaultButton(QMessageBox::Ok);
                    return box.exec() == QMessageBox::Ok;
                };
                if (QThread::currentThread() == thread()) {
                    return show_dialog();
                }
                bool decision = false;
                QMetaObject::invokeMethod(
                    this,
                    [&decision, show_dialog]() mutable { decision = show_dialog(); },
                    Qt::BlockingQueuedConnection);
                return decision;
            };

            auto update_cached_image_suggestion = [&](const FileEntry& entry,
                                                      const std::string& suggested_name) {
                const auto it = cached_visual_indices.find(entry.file_name);
                if (it == cached_visual_indices.end()) {
                    return;
                }
                auto& cached_entry = already_categorized_files[it->second];
                if (cached_entry.suggested_name == suggested_name) {
                    return;
                }
                cached_entry.suggested_name = suggested_name;
                persist_cached_suggestion(cached_entry, suggested_name);
            };

            auto handle_visual_failure = [&](const FileEntry& entry,
                                             const std::string& reason,
                                             bool already_renamed,
                                             bool log_failure,
                                             bool visual_only) {
                if (log_failure) {
                    append_progress(to_utf8(tr("[VISION-ERROR] %1 (%2)")
                                                .arg(QString::fromStdString(entry.file_name),
                                                     QString::fromStdString(reason))));
                }
                if (!rename_images_only && !visual_only) {
                    other_entries.push_back(entry);
                }
                const std::string suggested_name = already_renamed ? std::string() : entry.file_name;
                if (offer_image_renames || rename_images_only) {
                    image_info.emplace(
                        entry.file_name,
                        ImageAnalysisInfo{suggested_name, entry.file_name, entry.full_path});
                    if (rename_images_only) {
                        persist_rename_only_progress(entry, suggested_name);
                    }
                }
                if (visual_only) {
                    update_cached_image_suggestion(entry, suggested_name);
                }
            };

            auto create_analyzer = [&]() -> std::unique_ptr<LlavaImageAnalyzer> {
                return std::make_unique<LlavaImageAnalyzer>(
                    visual_paths->model_path,
                    visual_paths->mmproj_path,
                    vision_settings);
            };

            std::unique_ptr<LlavaImageAnalyzer> analyzer;
            bool skip_visual_analysis = false;
            std::string skip_visual_reason;
            try {
                analyzer = create_analyzer();
            } catch (const std::exception& ex) {
                if (!allow_visual_cpu_fallback) {
                    throw;
                }
                if (!visual_cpu_fallback_choice.has_value()) {
                    visual_cpu_fallback_choice = prompt_visual_cpu_fallback();
                }
                if (!visual_cpu_fallback_choice.value()) {
                    skip_visual_analysis = true;
                    skip_visual_reason = ex.what();
                } else {
                    append_progress(to_utf8(tr("[VISION] Switching visual analysis to CPU.")));
                    vision_settings.use_gpu = false;
                    visual_cpu_fallback_active = true;
                    try {
                        analyzer = create_analyzer();
                    } catch (const std::exception& init_ex) {
                        skip_visual_analysis = true;
                        skip_visual_reason = init_ex.what();
                    }
                }
            }

            if (skip_visual_analysis) {
                if (!skip_visual_reason.empty()) {
                    append_progress(to_utf8(tr("[VISION-ERROR] %1")
                                                .arg(QString::fromStdString(skip_visual_reason))));
                }
                append_progress(to_utf8(tr("[VISION] Visual analysis disabled; falling back to filenames.")));
                for (const auto& entry : image_entries) {
                    if (update_stop()) {
                        break;
                    }
                    const bool already_renamed = renamed_files.contains(entry.file_name);
                    if (already_renamed && rename_images_only) {
                        continue;
                    }
                    const bool visual_only = cached_visual_indices.contains(entry.file_name);
                    analyzed_image_entries.push_back(entry);
                    handle_visual_failure(entry, std::string(), already_renamed, false, visual_only);
                }
            } else {
                bool stop_visual_analysis = false;
                for (size_t index = 0; index < image_entries.size(); ++index) {
                    const auto& entry = image_entries[index];
                    if (update_stop()) {
                        break;
                    }
                    const bool already_renamed = renamed_files.contains(entry.file_name);
                    if (already_renamed && rename_images_only) {
                        continue;
                    }
                    const bool visual_only = cached_visual_indices.contains(entry.file_name);
                    const auto cached_suggestion_it = cached_image_suggestions.find(entry.file_name);
                    const bool has_cached_suggestion = cached_suggestion_it != cached_image_suggestions.end();
                    analyzed_image_entries.push_back(entry);

                    while (true) {
                try {
                    if (has_cached_suggestion) {
                        append_progress(to_utf8(tr("[VISION] Using cached suggestion for %1")
                                                    .arg(QString::fromStdString(entry.file_name))));
                        const std::string suggested_name = already_renamed ? std::string()
                                                                           : cached_suggestion_it->second;
                        const auto entry_path = Utils::utf8_to_path(entry.full_path);
                        const auto prompt_path = Utils::path_to_utf8(
                            entry_path.parent_path() / Utils::utf8_to_path(cached_suggestion_it->second));
                                image_info.emplace(
                                    entry.file_name,
                                    ImageAnalysisInfo{suggested_name, cached_suggestion_it->second, prompt_path});
                                if (rename_images_only) {
                                    persist_rename_only_progress(entry, suggested_name);
                                }
                                if (visual_only) {
                                    update_cached_image_suggestion(entry, suggested_name);
                                }
                                if (!rename_images_only && !visual_only) {
                                    image_entries_for_llm.push_back(entry);
                                }
                                break;
                            }
                            append_progress(to_utf8(tr("[VISION] Analyzing %1")
                                                        .arg(QString::fromStdString(entry.file_name))));
                            const auto analysis = analyzer->analyze(entry.full_path);
                            const auto entry_path = Utils::utf8_to_path(entry.full_path);
                            const auto prompt_path = Utils::path_to_utf8(
                                entry_path.parent_path() / Utils::utf8_to_path(analysis.suggested_name));

                            const std::string suggested_name = already_renamed ? std::string() : analysis.suggested_name;
                            image_info.emplace(
                                entry.file_name,
                                ImageAnalysisInfo{suggested_name, analysis.suggested_name, prompt_path});
                            if (rename_images_only) {
                                persist_rename_only_progress(entry, suggested_name);
                            }
                            if (visual_only) {
                                update_cached_image_suggestion(entry, suggested_name);
                            }

                            if (!rename_images_only && !visual_only) {
                                image_entries_for_llm.push_back(entry);
                            }
                            break;
                        } catch (const std::exception& ex) {
                            if (!visual_cpu_fallback_active &&
                                allow_visual_cpu_fallback &&
                                should_retry_on_cpu(ex)) {
                                if (!visual_cpu_fallback_choice.has_value()) {
                                    visual_cpu_fallback_choice = prompt_visual_cpu_fallback();
                                }
                                if (visual_cpu_fallback_choice.value()) {
                                    append_progress(to_utf8(tr("[VISION] GPU memory issue detected. Switching to CPU.")));
                                    vision_settings.use_gpu = false;
                                    visual_cpu_fallback_active = true;
                                    try {
                                        analyzer = create_analyzer();
                                    } catch (const std::exception& init_ex) {
                                        handle_visual_failure(entry, init_ex.what(), already_renamed, true, visual_only);
                                        append_progress(to_utf8(tr("[VISION] Visual analysis disabled for remaining images.")));
                                        stop_visual_analysis = true;
                                    }
                                    if (!stop_visual_analysis) {
                                        continue;
                                    }
                                } else {
                                    append_progress(to_utf8(tr("[VISION] Visual analysis disabled; falling back to filenames.")));
                                    handle_visual_failure(entry, ex.what(), already_renamed, true, visual_only);
                                    stop_visual_analysis = true;
                                }
                            } else {
                                handle_visual_failure(entry, ex.what(), already_renamed, true, visual_only);
                            }
                            break;
                        }
                    }

                    if (stop_visual_analysis) {
                        for (size_t remaining = index + 1; remaining < image_entries.size(); ++remaining) {
                            if (update_stop()) {
                                break;
                            }
                            const auto& pending = image_entries[remaining];
                            const bool pending_renamed = renamed_files.contains(pending.file_name);
                            if (pending_renamed && rename_images_only) {
                                continue;
                            }
                            const bool pending_visual_only = cached_visual_indices.contains(pending.file_name);
                            analyzed_image_entries.push_back(pending);
                            handle_visual_failure(pending, std::string(), pending_renamed, false, pending_visual_only);
                        }
                        break;
                    }
                }
            }
        }

        if (analyze_documents && !document_entries.empty()) {
            auto update_cached_document_suggestion = [&](const FileEntry& entry,
                                                         const std::string& suggested_name) {
                const auto it = cached_document_indices.find(entry.file_name);
                if (it == cached_document_indices.end()) {
                    return;
                }
                auto& cached_entry = already_categorized_files[it->second];
                if (cached_entry.suggested_name == suggested_name) {
                    return;
                }
                cached_entry.suggested_name = suggested_name;
                persist_cached_suggestion(cached_entry, suggested_name);
            };

            auto handle_document_failure = [&](const FileEntry& entry,
                                               const std::string& reason,
                                               bool already_renamed,
                                               bool log_failure,
                                               bool document_only) {
                if (log_failure) {
                    append_progress(to_utf8(tr("[DOC-ERROR] %1 (%2)")
                                                .arg(QString::fromStdString(entry.file_name),
                                                     QString::fromStdString(reason))));
                }
                if (!rename_documents_only && !document_only) {
                    other_entries.push_back(entry);
                }
                const std::string suggested_name = already_renamed ? std::string() : entry.file_name;
                if (offer_document_renames || rename_documents_only) {
                    document_info.emplace(
                        entry.file_name,
                        DocumentAnalysisInfo{suggested_name, entry.file_name, entry.full_path});
                    if (rename_documents_only) {
                        persist_rename_only_progress(entry, suggested_name);
                    }
                }
                if (document_only) {
                    update_cached_document_suggestion(entry, suggested_name);
                }
            };

            DocumentTextAnalyzer::Settings doc_settings;
            doc_settings.max_tokens = 256;
            const size_t char_budget = resolve_document_char_budget(using_local_llm, doc_settings.max_tokens);
            doc_settings.max_characters = std::min(doc_settings.max_characters, char_budget);
            DocumentTextAnalyzer doc_analyzer(doc_settings);

            auto llm = make_llm_client();
            if (!llm) {
                throw std::runtime_error("Failed to create LLM client.");
            }
            llm->set_prompt_logging_enabled(should_log_prompts());

            for (const auto& entry : document_entries) {
                if (update_stop()) {
                    break;
                }
                const bool already_renamed = renamed_files.contains(entry.file_name);
                if (already_renamed && rename_documents_only) {
                    continue;
                }
                const bool document_only = cached_document_indices.contains(entry.file_name);
                const auto cached_suggestion_it = cached_document_suggestions.find(entry.file_name);
                const bool has_cached_suggestion = cached_suggestion_it != cached_document_suggestions.end();
                if (add_document_date && !document_dates.contains(entry.file_name)) {
                    const auto date = DocumentTextAnalyzer::extract_creation_date(
                        Utils::utf8_to_path(entry.full_path));
                    if (date) {
                        document_dates.emplace(entry.file_name, *date);
                    }
                }
                analyzed_document_entries.push_back(entry);

                try {
                    if (has_cached_suggestion) {
                        append_progress(to_utf8(tr("[DOC] Using cached suggestion for %1")
                                                    .arg(QString::fromStdString(entry.file_name))));
                        const std::string suggested_name = already_renamed ? std::string()
                                                                           : cached_suggestion_it->second;
                        document_info.emplace(
                            entry.file_name,
                            DocumentAnalysisInfo{suggested_name,
                                                 entry.file_name,
                                                 entry.full_path});
                        if (rename_documents_only) {
                            persist_rename_only_progress(entry, suggested_name);
                        }
                        if (!rename_documents_only && !document_only) {
                            document_entries_for_llm.push_back(entry);
                        }
                        continue;
                    }

                    append_progress(to_utf8(tr("[DOC] Analyzing %1")
                                                .arg(QString::fromStdString(entry.file_name))));
                    const auto analysis = doc_analyzer.analyze(Utils::utf8_to_path(entry.full_path), *llm);
                    const std::string suggested_name = already_renamed ? std::string() : analysis.suggested_name;
                    std::string prompt_path = entry.full_path;
                    if (!analysis.summary.empty()) {
                        prompt_path += "\nDocument summary: " + analysis.summary;
                    }
                    document_info.emplace(
                        entry.file_name,
                        DocumentAnalysisInfo{suggested_name,
                                             analysis.suggested_name.empty() ? entry.file_name : analysis.suggested_name,
                                             prompt_path});
                    if (rename_documents_only) {
                        persist_rename_only_progress(entry, suggested_name);
                    }
                    if (document_only) {
                        update_cached_document_suggestion(entry, suggested_name);
                    }
                    if (!rename_documents_only && !document_only) {
                        document_entries_for_llm.push_back(entry);
                    }
                } catch (const std::exception& ex) {
                    handle_document_failure(entry, ex.what(), already_renamed, true, document_only);
                }
            }
        }

        update_stop();

        auto suggested_name_provider = [offer_image_renames, offer_document_renames,
                                        &image_info, &document_info](const FileEntry& entry) -> std::string {
            if (offer_image_renames) {
                if (const auto it = image_info.find(entry.file_name); it != image_info.end()) {
                    return it->second.suggested_name;
                }
            }
            if (offer_document_renames) {
                if (const auto it = document_info.find(entry.file_name); it != document_info.end()) {
                    return it->second.suggested_name;
                }
            }
            return std::string();
        };

        auto apply_document_dates = [this, add_document_date, &document_dates](std::vector<CategorizedFile>& results) {
            if (!add_document_date || document_dates.empty()) {
                return;
            }
            for (auto& entry : results) {
                if (entry.type != FileType::File) {
                    continue;
                }
                const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                       Utils::utf8_to_path(entry.file_name);
                if (!DocumentTextAnalyzer::is_supported_document(full_path)) {
                    continue;
                }
                auto it = document_dates.find(entry.file_name);
                if (it == document_dates.end()) {
                    if (const auto date = DocumentTextAnalyzer::extract_creation_date(full_path)) {
                        it = document_dates.emplace(entry.file_name, *date).first;
                    }
                }
                if (it == document_dates.end() || it->second.empty()) {
                    continue;
                }
                if (entry.category.empty()) {
                    continue;
                }
                const std::string suffix = "_" + it->second;
                if (entry.category.size() >= suffix.size() &&
                    entry.category.compare(entry.category.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    continue;
                }
                entry.category += suffix;

                DatabaseManager::ResolvedCategory resolved{entry.taxonomy_id, entry.category, entry.subcategory};
                const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
                db_manager.insert_or_update_file_with_categorization(
                    entry.file_name,
                    file_type_label,
                    entry.file_path,
                    resolved,
                    entry.used_consistency_hints,
                    entry.suggested_name,
                    entry.rename_only,
                    entry.rename_applied);
            }
        };

        std::vector<CategorizedFile> other_results;
        if (!stop_requested && !other_entries.empty()) {
            other_results = categorization_service.categorize_entries(
                other_entries,
                using_local_llm,
                stop_analysis,
                [this](const std::string& message) { append_progress(message); },
                [this](const FileEntry& entry) {
                    const QString type_label = entry.type == FileType::Directory ? tr("Directory") : tr("File");
                    append_progress(to_utf8(tr("[SORT] %1 (%2)")
                                                .arg(QString::fromStdString(entry.file_name), type_label)));
                },
                [this](const CategorizedFile& entry, const std::string& reason) {
                    notify_recategorization_reset(entry, reason);
                },
                [this]() { return make_llm_client(); },
                {},
                suggested_name_provider);
        }
        apply_document_dates(other_results);
        update_stop();

        std::vector<CategorizedFile> image_results;
        if (analyze_images && !analyzed_image_entries.empty()) {
            if (rename_images_only) {
                image_results.reserve(analyzed_image_entries.size());
                for (const auto& entry : analyzed_image_entries) {
                    const auto entry_path = Utils::utf8_to_path(entry.full_path);
                    CategorizedFile result{Utils::path_to_utf8(entry_path.parent_path()),
                                           entry.file_name,
                                           entry.type,
                                           "", "", 0};
                    result.rename_only = true;
                    if (offer_image_renames || rename_images_only) {
                        const auto info_it = image_info.find(entry.file_name);
                        if (info_it != image_info.end()) {
                            result.suggested_name = info_it->second.suggested_name;
                        }
                    }
                    image_results.push_back(std::move(result));
                }
            } else if (!image_entries_for_llm.empty()) {
                const bool stop_before_image_categorization = stop_requested;
                const bool bypass_stop = stop_before_image_categorization && other_results.empty();
                std::atomic<bool> image_stop{false};
                std::atomic<bool>& stop_flag = bypass_stop ? image_stop : stop_analysis;

                auto override_provider = [&image_info](const FileEntry& entry)
                    -> std::optional<CategorizationService::PromptOverride> {
                    const auto it = image_info.find(entry.file_name);
                    if (it == image_info.end()) {
                        return std::nullopt;
                    }
                    return CategorizationService::PromptOverride{it->second.prompt_name, it->second.prompt_path};
                };

                image_results = categorization_service.categorize_entries(
                    image_entries_for_llm,
                    using_local_llm,
                    stop_flag,
                    [this](const std::string& message) { append_progress(message); },
                    [this](const FileEntry& entry) {
                        const QString type_label = entry.type == FileType::Directory ? tr("Directory") : tr("File");
                        append_progress(to_utf8(tr("[SORT] %1 (%2)")
                                                    .arg(QString::fromStdString(entry.file_name), type_label)));
                    },
                    [this](const CategorizedFile& entry, const std::string& reason) {
                        notify_recategorization_reset(entry, reason);
                    },
                    [this]() { return make_llm_client(); },
                    override_provider,
                    suggested_name_provider);

                update_stop();
            }
        }

        std::vector<CategorizedFile> document_results;
        if (analyze_documents && !analyzed_document_entries.empty()) {
            if (rename_documents_only) {
                document_results.reserve(analyzed_document_entries.size());
                for (const auto& entry : analyzed_document_entries) {
                    const auto entry_path = Utils::utf8_to_path(entry.full_path);
                    CategorizedFile result{Utils::path_to_utf8(entry_path.parent_path()),
                                           entry.file_name,
                                           entry.type,
                                           "", "", 0};
                    result.rename_only = true;
                    if (offer_document_renames || rename_documents_only) {
                        const auto info_it = document_info.find(entry.file_name);
                        if (info_it != document_info.end()) {
                            result.suggested_name = info_it->second.suggested_name;
                        }
                    }
                    document_results.push_back(std::move(result));
                }
            } else if (!document_entries_for_llm.empty()) {
                const bool stop_before_doc_categorization = stop_requested;
                const bool bypass_stop = stop_before_doc_categorization &&
                                         other_results.empty() &&
                                         image_results.empty();
                std::atomic<bool> doc_stop{false};
                std::atomic<bool>& stop_flag = bypass_stop ? doc_stop : stop_analysis;

                auto override_provider = [&document_info](const FileEntry& entry)
                    -> std::optional<CategorizationService::PromptOverride> {
                    const auto it = document_info.find(entry.file_name);
                    if (it == document_info.end()) {
                        return std::nullopt;
                    }
                    return CategorizationService::PromptOverride{it->second.prompt_name, it->second.prompt_path};
                };

                document_results = categorization_service.categorize_entries(
                    document_entries_for_llm,
                    using_local_llm,
                    stop_flag,
                    [this](const std::string& message) { append_progress(message); },
                    [this](const FileEntry& entry) {
                        const QString type_label = entry.type == FileType::Directory ? tr("Directory") : tr("File");
                        append_progress(to_utf8(tr("[SORT] %1 (%2)")
                                                    .arg(QString::fromStdString(entry.file_name), type_label)));
                    },
                    [this](const CategorizedFile& entry, const std::string& reason) {
                        notify_recategorization_reset(entry, reason);
                    },
                    [this]() { return make_llm_client(); },
                    override_provider,
                    suggested_name_provider);
            }
        }

        apply_document_dates(document_results);

        update_stop();

        new_files_with_categories.clear();
        new_files_with_categories.reserve(other_results.size() + image_results.size() + document_results.size());
        new_files_with_categories.insert(new_files_with_categories.end(),
                                         other_results.begin(),
                                         other_results.end());
        new_files_with_categories.insert(new_files_with_categories.end(),
                                         image_results.begin(),
                                         image_results.end());
        new_files_with_categories.insert(new_files_with_categories.end(),
                                         document_results.begin(),
                                         document_results.end());

        core_logger->info("Categorization produced {} new record(s).",
                          new_files_with_categories.size());

        already_categorized_files.insert(
            already_categorized_files.end(),
            new_files_with_categories.begin(),
            new_files_with_categories.end());

        std::vector<CategorizedFile> review_entries = already_categorized_files;
        if ((rename_images_only || rename_documents_only) && !pending_renames.empty()) {
            review_entries.insert(review_entries.end(),
                                  pending_renames.begin(),
                                  pending_renames.end());
        }

        const auto actual_files = results_coordinator.list_directory(get_folder_path(), scan_options);
        new_files_to_sort = results_coordinator.compute_files_to_sort(get_folder_path(),
                                                                      scan_options,
                                                                      actual_files,
                                                                      review_entries);
        core_logger->debug("{} file(s) queued for sorting after analysis.",
                           new_files_to_sort.size());

        const bool cancelled = stop_requested;
        run_on_ui([this, cancelled]() {
            if (cancelled && new_files_to_sort.empty()) {
                handle_analysis_cancelled();
            } else {
                handle_analysis_finished();
            }
        });
    } catch (const std::exception& ex) {
        core_logger->error("Exception during analysis: {}", ex.what());
        run_on_ui([this, message = std::string("Analysis error: ") + ex.what()]() {
            handle_analysis_failure(message);
        });
    }
}


void MainApp::run_consistency_pass()
{
    if (stop_analysis.load() || already_categorized_files.empty()) {
        return;
    }

    auto progress_sink = [this](const std::string& message) {
        run_on_ui([this, message]() {
            if (progress_dialog) {
                progress_dialog->append_text(message);
            }
        });
    };

    consistency_pass_service.run(
        already_categorized_files,
        new_files_with_categories,
        [this]() { return make_llm_client(); },
        stop_analysis,
        progress_sink);
}

void MainApp::handle_development_prompt_logging(bool checked)
{
    if (!development_mode_) {
        if (development_prompt_logging_action) {
            QSignalBlocker blocker(development_prompt_logging_action);
            development_prompt_logging_action->setChecked(false);
        }
        development_prompt_logging_enabled_ = false;
        apply_development_logging();
        return;
    }

    development_prompt_logging_enabled_ = checked;
    settings.set_development_prompt_logging(checked);
    apply_development_logging();
}

void MainApp::request_stop_analysis()
{
    stop_analysis = true;
    statusBar()->showMessage(tr("Cancelling analysis…"), 4000);
    status_is_ready_ = false;
}


void MainApp::stop_running_analysis()
{
    stop_analysis = true;
    if (analyze_thread.joinable()) {
        analyze_thread.join();
    }
    if (progress_dialog) {
        progress_dialog->hide();
        progress_dialog.reset();
    }
}


void MainApp::show_llm_selection_dialog()
{
    try {
        auto dialog = std::make_unique<LLMSelectionDialog>(settings, this);
        if (dialog->exec() == QDialog::Accepted) {
            settings.set_openai_api_key(dialog->get_openai_api_key());
            settings.set_openai_model(dialog->get_openai_model());
            settings.set_gemini_api_key(dialog->get_gemini_api_key());
            settings.set_gemini_model(dialog->get_gemini_model());
            settings.set_llm_choice(dialog->get_selected_llm_choice());
            if (dialog->get_selected_llm_choice() == LLMChoice::Custom) {
                settings.set_active_custom_llm_id(dialog->get_selected_custom_llm_id());
            } else {
                settings.set_active_custom_llm_id("");
            }
            if (dialog->get_selected_llm_choice() == LLMChoice::Remote_Custom) {
                settings.set_active_custom_api_id(dialog->get_selected_custom_api_id());
            } else {
                settings.set_active_custom_api_id("");
            }
            using_local_llm = !is_remote_choice(settings.get_llm_choice());
            settings.save();
        }
    } catch (const std::exception& ex) {
        show_error_dialog(fmt::format("LLM selection error: {}", ex.what()));
    }
}


void MainApp::on_about_activate()
{
    MainAppHelpActions::show_about(this);
}

bool MainApp::should_log_prompts() const
{
    return development_mode_ && development_prompt_logging_enabled_;
}

void MainApp::apply_development_logging()
{
    consistency_pass_service.set_prompt_logging_enabled(should_log_prompts());
}


std::unique_ptr<ILLMClient> MainApp::make_llm_client()
{
    const LLMChoice choice = settings.get_llm_choice();

    if (choice == LLMChoice::Remote_OpenAI) {
        const std::string api_key = settings.get_openai_api_key();
        const std::string model = settings.get_openai_model();
        if (api_key.empty()) {
            throw std::runtime_error("OpenAI API key is missing. Please add it from Select LLM.");
        }
        CategorizationSession session(api_key, model);
        auto client = std::make_unique<LLMClient>(session.create_llm_client());
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    if (choice == LLMChoice::Remote_Gemini) {
        const std::string api_key = settings.get_gemini_api_key();
        const std::string model = settings.get_gemini_model();
        if (api_key.empty()) {
            throw std::runtime_error("Gemini API key is missing. Please add it from Select LLM.");
        }
        auto client = std::make_unique<GeminiClient>(api_key, model);
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    if (choice == LLMChoice::Remote_Custom) {
        const auto id = settings.get_active_custom_api_id();
        const CustomApiEndpoint endpoint = settings.find_custom_api_endpoint(id);
        if (endpoint.id.empty() || endpoint.base_url.empty() || endpoint.model.empty()) {
            throw std::runtime_error("Selected custom API endpoint is missing or invalid. Please re-select it.");
        }
        auto client = std::make_unique<LLMClient>(endpoint.api_key, endpoint.model, endpoint.base_url);
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    if (choice == LLMChoice::Custom) {
        const auto id = settings.get_active_custom_llm_id();
        const CustomLLM custom = settings.find_custom_llm(id);
        if (custom.id.empty() || custom.path.empty()) {
            throw std::runtime_error("Selected custom LLM is missing or invalid. Please re-select it.");
        }
        auto client = std::make_unique<LocalLLMClient>(custom.path);
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    const char* env_var = nullptr;
    switch (choice) {
        case LLMChoice::Local_3b:
            env_var = "LOCAL_LLM_3B_DOWNLOAD_URL";
            break;
        case LLMChoice::Local_3b_legacy:
            env_var = "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL";
            break;
        case LLMChoice::Local_7b:
            env_var = "LOCAL_LLM_7B_DOWNLOAD_URL";
            break;
        default:
            break;
    }

    const char* env_url = env_var ? std::getenv(env_var) : nullptr;
    if (!env_url) {
        throw std::runtime_error("Required environment variable for selected model is not set");
    }

    auto client = std::make_unique<LocalLLMClient>(
        Utils::make_default_path_to_file_from_download_url(env_url));
    client->set_prompt_logging_enabled(should_log_prompts());
    return client;
}

void MainApp::notify_recategorization_reset(const std::vector<CategorizedFile>& entries,
                                            const std::string& reason)
{
    if (entries.empty()) {
        return;
    }

    auto shared_entries = std::make_shared<std::vector<CategorizedFile>>(entries);
    auto shared_reason = std::make_shared<std::string>(reason);

    run_on_ui([this, shared_entries, shared_reason]() {
        if (!progress_dialog) {
            return;
        }
        for (const auto& entry : *shared_entries) {
            const QString message = tr("[WARN] %1 will be re-categorized: %2")
                                        .arg(QString::fromStdString(entry.file_name),
                                             QString::fromStdString(*shared_reason));
            progress_dialog->append_text(to_utf8(message));
        }
    });
}

void MainApp::notify_recategorization_reset(const CategorizedFile& entry,
                                            const std::string& reason)
{
    notify_recategorization_reset(std::vector<CategorizedFile>{entry}, reason);
}




void MainApp::show_results_dialog(const std::vector<CategorizedFile>& results)
{
    try {
        const bool show_subcategory = use_subcategories_checkbox->isChecked();
        const std::string undo_dir = settings.get_config_dir() + "/undo";
        categorization_dialog = std::make_unique<CategorizationDialog>(&db_manager, show_subcategory, undo_dir, this);
        categorization_dialog->show_results(results);

        const int newly_analyzed = static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const CategorizedFile& file) { return !file.from_cache; }));
        if (newly_analyzed > 0) {
            record_categorized_metrics(newly_analyzed);
        }
    } catch (const std::exception& ex) {
        if (ui_logger) {
            ui_logger->error("Error showing results dialog: {}", ex.what());
        }
        show_error_dialog(fmt::format("Failed to show results dialog: {}", ex.what()));
    }
}


void MainApp::show_error_dialog(const std::string& message)
{
    DialogUtils::show_error_dialog(this, message);
}


void MainApp::report_progress(const std::string& message)
{
    run_on_ui([this, message]() {
        if (progress_dialog) {
            progress_dialog->append_text(message);
        }
    });
}


std::string MainApp::get_folder_path() const
{
    const QByteArray bytes = path_entry->text().toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}


void MainApp::run_on_ui(std::function<void()> func)
{
    QMetaObject::invokeMethod(
        this,
        [fn = std::move(func)]() mutable {
            if (fn) {
                fn();
            }
        },
        Qt::QueuedConnection);
}

void MainApp::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange) {
        retranslate_ui();
    }
}


void MainApp::closeEvent(QCloseEvent* event)
{
    stop_running_analysis();
    save_settings();
    QMainWindow::closeEvent(event);
}
