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

#include <chrono>
#include <filesystem>
#include <algorithm>
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
        increment = 200;
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
        threshold = ((base / 200) + 1) * 200;
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

    int increment = 200;
    if (result == MainApp::SupportPromptResult::Support ||
        result == MainApp::SupportPromptResult::CannotDonate) {
        increment = 750;
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

    using_local_llm = settings.get_llm_choice() != LLMChoice::Remote;

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

    if (!Utils::is_network_available()) {
        show_error_dialog(ERR_NO_INTERNET_CONNECTION);
        core_logger->warn("Network unavailable when attempting to analyze '{}'", folder_path);
        return;
    }

    if (!using_local_llm) {
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
    append_progress("[ARCHIVE] Already categorized highlights:");
    for (const auto& file_entry : already_categorized_files) {
        const char* symbol = file_entry.type == FileType::Directory ? "DIR" : "FILE";
        const std::string sub = file_entry.subcategory.empty() ? "-" : file_entry.subcategory;
        append_progress(fmt::format("  - [{}] {} -> {} / {}", symbol, file_entry.file_name, file_entry.category, sub));
    }
}

void MainApp::log_pending_queue()
{
    if (!progress_dialog) {
        return;
    }
    if (files_to_categorize.empty()) {
        append_progress("[DONE] No files to categorize.");
        return;
    }

    append_progress("[QUEUE] Items waiting for categorization:");
    for (const auto& file_entry : files_to_categorize) {
        const char* symbol = file_entry.type == FileType::Directory ? "DIR" : "FILE";
        append_progress(fmt::format("  - [{}] {}", symbol, file_entry.file_name));
    }
}

void MainApp::perform_analysis()
{
    const std::string directory_path = get_folder_path();
    core_logger->info("Starting analysis for directory '{}'", directory_path);

    append_progress(fmt::format("[SCAN] Exploring {}", directory_path));
    if (should_abort_analysis()) {
        return;
    }

    try {
        prune_empty_cached_entries_for(directory_path);
        already_categorized_files = categorization_service.load_cached_entries(directory_path);

        if (should_abort_analysis()) {
            return;
        }

        log_cached_highlights();

        const auto cached_file_names = results_coordinator.extract_file_names(already_categorized_files);
        files_to_categorize = results_coordinator.find_files_to_categorize(directory_path, file_scan_options, cached_file_names);
        core_logger->debug("Found {} item(s) pending categorization in '{}'.",
                           files_to_categorize.size(), directory_path);

        log_pending_queue();
        if (should_abort_analysis()) {
            return;
        }

        append_progress("[PROCESS] Letting the AI do its magic...");

        new_files_with_categories = categorization_service.categorize_entries(
            files_to_categorize,
            using_local_llm,
            stop_analysis,
            [this](const std::string& message) { append_progress(message); },
            [this](const FileEntry& entry) {
                append_progress(fmt::format("[SORT] {} ({})",
                                            entry.file_name,
                                            entry.type == FileType::Directory ? "directory" : "file"));
            },
            [this](const CategorizedFile& entry, const std::string& reason) {
                notify_recategorization_reset(entry, reason);
            },
            [this]() { return make_llm_client(); });

        core_logger->info("Categorization produced {} new record(s).",
                          new_files_with_categories.size());

        already_categorized_files.insert(
            already_categorized_files.end(),
            new_files_with_categories.begin(),
            new_files_with_categories.end());

        const auto actual_files = results_coordinator.list_directory(get_folder_path(), file_scan_options);
        new_files_to_sort = results_coordinator.compute_files_to_sort(get_folder_path(), file_scan_options, actual_files, already_categorized_files);
        core_logger->debug("{} file(s) queued for sorting after analysis.",
                           new_files_to_sort.size());

        run_on_ui([this]() {
            handle_analysis_finished();
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
            settings.set_remote_api_key(dialog->get_remote_api_key());
            settings.set_remote_model(dialog->get_remote_model());
            settings.set_gemini_api_key(dialog->get_gemini_api_key());
            settings.set_gemini_model(dialog->get_gemini_model());
            settings.set_llm_choice(dialog->get_selected_llm_choice());
            if (dialog->get_selected_llm_choice() == LLMChoice::Custom) {
                settings.set_active_custom_llm_id(dialog->get_selected_custom_llm_id());
            } else {
                settings.set_active_custom_llm_id("");
            }
            using_local_llm = (settings.get_llm_choice() != LLMChoice::Remote && 
                              settings.get_llm_choice() != LLMChoice::Gemini);
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
    if (settings.get_llm_choice() == LLMChoice::Remote) {
        const std::string api_key = settings.get_remote_api_key();
        const std::string model = settings.get_remote_model();
        if (api_key.empty()) {
            throw std::runtime_error("OpenAI API key is missing. Please add it from Select LLM.");
        }
        auto client = std::make_unique<LLMClient>(api_key, model);
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    if (settings.get_llm_choice() == LLMChoice::Gemini) {
        const std::string api_key = settings.get_gemini_api_key();
        const std::string model = settings.get_gemini_model();
        if (api_key.empty()) {
            throw std::runtime_error("Gemini API key is missing. Please add it from Select LLM.");
        }
        auto client = std::make_unique<GeminiClient>(api_key, model);
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    if (settings.get_llm_choice() == LLMChoice::Custom) {
        const auto id = settings.get_active_custom_llm_id();
        const CustomLLM custom = settings.find_custom_llm(id);
        if (custom.id.empty() || custom.path.empty()) {
            throw std::runtime_error("Selected custom LLM is missing or invalid. Please re-select it.");
        }
        auto client = std::make_unique<LocalLLMClient>(custom.path);
        client->set_prompt_logging_enabled(should_log_prompts());
        return client;
    }

    const char* env_var = settings.get_llm_choice() == LLMChoice::Local_3b
        ? "LOCAL_LLM_3B_DOWNLOAD_URL"
        : "LOCAL_LLM_7B_DOWNLOAD_URL";

    const char* env_url = std::getenv(env_var);
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
            progress_dialog->append_text(
                fmt::format("[WARN] {} will be re-categorized: {}",
                            entry.file_name,
                            *shared_reason));
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
