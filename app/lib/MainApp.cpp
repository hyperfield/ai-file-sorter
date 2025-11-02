#include "MainApp.hpp"

#include "CategorizationSession.hpp"
#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "LLMClient.hpp"
#include "LLMSelectionDialog.hpp"
#include "Logger.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "Updater.hpp"
#include "TranslationManager.hpp"
#include "Utils.hpp"
#include "Types.hpp"
#include "MainAppUiBuilder.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QFileSystemModel>
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
#include <QStandardItem>
#include <QStandardItemModel>
#include <QCoreApplication>
#include <QStringList>
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

MainApp::MainApp(Settings& settings, QWidget* parent)
    : QMainWindow(parent),
      settings(settings),
      db_manager(settings.get_config_dir()),
      core_logger(Logger::get_logger("core_logger")),
      ui_logger(Logger::get_logger("ui_logger")),
      categorization_service(settings, db_manager, core_logger),
      consistency_pass_service(db_manager, core_logger),
      results_coordinator(dirscanner)
{
    TranslationManager::instance().initialize(qApp);
    TranslationManager::instance().set_language(settings.get_language());

    if (settings.get_llm_choice() != LLMChoice::Remote) {
        using_local_llm = true;
    }

    MainAppUiBuilder ui_builder;
    ui_builder.build(*this);
    retranslate_ui();
    setup_file_explorer();
    connect_signals();
    connect_edit_actions();
    start_updater();
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
    file_explorer_dock = new QDockWidget(tr("File Explorer"), this);
    file_explorer_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, file_explorer_dock);

    file_system_model = new QFileSystemModel(file_explorer_dock);
    const QString root_path = QDir::rootPath();
    file_system_model->setRootPath(root_path);
    file_system_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Drives | QDir::AllDirs);

    file_explorer_view = new QTreeView(file_explorer_dock);
    file_explorer_view->setModel(file_system_model);
    file_explorer_view->setRootIndex(file_system_model->index(root_path));
    const QModelIndex home_index = file_system_model->index(QDir::homePath());
    if (home_index.isValid()) {
        file_explorer_view->setCurrentIndex(home_index);
        file_explorer_view->scrollTo(home_index);
    }
    file_explorer_view->setHeaderHidden(false);
    file_explorer_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    file_explorer_view->setColumnHidden(1, true);
    file_explorer_view->setColumnHidden(2, true);
    file_explorer_view->setColumnHidden(3, true);

    connect(file_explorer_view, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!file_system_model->isDir(index)) {
            return;
        }
        on_directory_selected(file_system_model->filePath(index));
    });

    connect(file_explorer_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                if (!file_system_model) {
                    return;
                }
                if (!current.isValid() || !file_system_model->isDir(current)) {
                    return;
                }
                update_folder_contents(file_system_model->filePath(current));
            });

    connect(file_explorer_dock, &QDockWidget::visibilityChanged, this, [this](bool) {
        update_results_view_mode();
    });

    file_explorer_dock->setWidget(file_explorer_view);

    const bool show_explorer = settings.get_show_file_explorer();
    if (file_explorer_menu_action) {
        file_explorer_menu_action->setChecked(show_explorer);
    }
    // if (consistency_pass_action) {
    //     consistency_pass_action->setChecked(settings.get_consistency_pass_enabled());
    // }
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
            statusBar()->showMessage(tr("Set folder to %1").arg(folder), 3000);
            status_is_ready_ = false;
        } else {
            show_error_dialog(ERR_INVALID_PATH);
        }
    });

    connect(use_subcategories_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        settings.set_use_subcategories(checked);
    });

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
    use_subcategories_checkbox->setChecked(settings.get_use_subcategories());
    categorize_files_checkbox->setChecked(settings.get_categorize_files());
    categorize_directories_checkbox->setChecked(settings.get_categorize_directories());

    const std::string sort_folder = settings.get_sort_folder();
    path_entry->setText(QString::fromStdString(sort_folder));

    if (QDir(QString::fromStdString(sort_folder)).exists()) {
        statusBar()->showMessage(tr("Loaded folder %1").arg(QString::fromStdString(sort_folder)), 3000);
        status_is_ready_ = false;
        update_folder_contents(QString::fromStdString(sort_folder));
    } else if (!sort_folder.empty()) {
        core_logger->warn("Sort folder path is invalid: {}", sort_folder);
    }

    file_scan_options = FileScanOptions::None;
    if (settings.get_categorize_files()) {
        file_scan_options = file_scan_options | FileScanOptions::Files;
    }
    if (settings.get_categorize_directories()) {
        file_scan_options = file_scan_options | FileScanOptions::Directories;
    }

    const bool show_explorer = settings.get_show_file_explorer();
    if (file_explorer_dock) {
        file_explorer_dock->setVisible(show_explorer);
    }
    if (file_explorer_menu_action) {
        file_explorer_menu_action->setChecked(show_explorer);
    }
    update_results_view_mode();

    update_language_checks();
}


void MainApp::sync_ui_to_settings()
{
    settings.set_use_subcategories(use_subcategories_checkbox->isChecked());
    settings.set_categorize_files(categorize_files_checkbox->isChecked());
    settings.set_categorize_directories(categorize_directories_checkbox->isChecked());
    const QByteArray folder_bytes = path_entry->text().toUtf8();
    settings.set_sort_folder(std::string(folder_bytes.constData(), static_cast<std::size_t>(folder_bytes.size())));
    if (file_explorer_menu_action) {
        settings.set_show_file_explorer(file_explorer_menu_action->isChecked());
    }
    // if (consistency_pass_action) {
    //     settings.set_consistency_pass_enabled(consistency_pass_action->isChecked());
    // }
    if (language_group) {
        if (QAction* checked = language_group->checkedAction()) {
            settings.set_language(static_cast<Language>(checked->data().toInt()));
        }
    }
}

void MainApp::retranslate_ui()
{
    setWindowTitle(QStringLiteral("AI File Sorter"));

    if (path_label) {
        path_label->setText(tr("Folder:"));
    }
    if (browse_button) {
        browse_button->setText(tr("Browse…"));
    }
    if (use_subcategories_checkbox) {
        use_subcategories_checkbox->setText(tr("Use subcategories"));
    }
    if (categorize_files_checkbox) {
        categorize_files_checkbox->setText(tr("Categorize files"));
    }
    if (categorize_directories_checkbox) {
        categorize_directories_checkbox->setText(tr("Categorize directories"));
    }
    if (analyze_button) {
        analyze_button->setText(analysis_in_progress_ ? tr("Stop analyzing") : tr("Analyze folder"));
    }

    if (tree_model) {
        tree_model->setHorizontalHeaderLabels(QStringList{
            tr("File"),
            tr("Type"),
            tr("Category"),
            tr("Subcategory"),
            tr("Status")
        });

        for (int row = 0; row < tree_model->rowCount(); ++row) {
            if (auto* type_item = tree_model->item(row, 1)) {
                const QString type_code = type_item->data(Qt::UserRole).toString();
                if (type_code == QStringLiteral("D")) {
                    type_item->setText(tr("Directory"));
                } else if (type_code == QStringLiteral("F")) {
                    type_item->setText(tr("File"));
                }
            }
            if (auto* status_item = tree_model->item(row, 4)) {
                const QString status_code = status_item->data(Qt::UserRole).toString();
                if (status_code == QStringLiteral("ready")) {
                    status_item->setText(tr("Ready"));
                }
            }
        }
    }

    if (file_menu) {
        file_menu->setTitle(tr("&File"));
    }
    if (file_quit_action) {
        file_quit_action->setText(tr("&Quit"));
    }
    if (edit_menu) {
        edit_menu->setTitle(tr("&Edit"));
    }
    if (copy_action) {
        copy_action->setText(tr("&Copy"));
    }
    if (cut_action) {
        cut_action->setText(tr("Cu&t"));
    }
    if (paste_action) {
        paste_action->setText(tr("&Paste"));
    }
    if (delete_action) {
        delete_action->setText(tr("&Delete"));
    }
    if (view_menu) {
        view_menu->setTitle(tr("&View"));
    }
    if (toggle_explorer_action) {
        toggle_explorer_action->setText(tr("File &Explorer"));
    }
    if (settings_menu) {
        settings_menu->setTitle(tr("&Settings"));
    }
    if (toggle_llm_action) {
        toggle_llm_action->setText(tr("Select &LLM…"));
    }
    if (consistency_pass_action) {
        consistency_pass_action->setText(tr("Run &consistency pass"));
    }
    if (language_menu) {
        language_menu->setTitle(tr("&Language"));
    }
    if (english_action) {
        english_action->setText(tr("&English"));
    }
    if (french_action) {
        french_action->setText(tr("&French"));
    }
    if (help_menu) {
        const QString help_title = QString(QChar(0x200B)) + tr("&Help");
        help_menu->setTitle(help_title);
        if (QAction* help_action = help_menu->menuAction()) {
            help_action->setText(help_title);
        }
    }
    if (about_action) {
        about_action->setText(tr("&About AI File Sorter"));
    }
    if (about_qt_action) {
        about_qt_action->setText(tr("About &Qt"));
    }
    if (about_agpl_action) {
        about_agpl_action->setText(tr("About &AGPL"));
    }
    if (file_explorer_dock) {
        file_explorer_dock->setWindowTitle(tr("File Explorer"));
    }

    if (analysis_in_progress_) {
        if (stop_analysis.load()) {
            statusBar()->showMessage(tr("Cancelling analysis…"), 4000);
        } else {
            statusBar()->showMessage(tr("Analyzing…"));
        }
    } else if (status_is_ready_) {
        statusBar()->showMessage(tr("Ready"));
    }

    update_language_checks();
}

void MainApp::update_language_checks()
{
    if (!language_group) {
        return;
    }

    Language configured = settings.get_language();
    if (configured != Language::English && configured != Language::French) {
        configured = Language::English;
        settings.set_language(configured);
    }

    QSignalBlocker blocker(language_group);
    if (english_action) {
        english_action->setChecked(configured == Language::English);
    }
    if (french_action) {
        french_action->setChecked(configured == Language::French);
    }
}

void MainApp::on_language_selected(Language language)
{
    settings.set_language(language);
    TranslationManager::instance().set_language(language);
    update_language_checks();
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


void MainApp::on_directory_selected(const QString& path)
{
    path_entry->setText(path);
    statusBar()->showMessage(tr("Folder selected: %1").arg(path), 3000);
    status_is_ready_ = false;

    if (file_system_model && file_explorer_view) {
        const QModelIndex index = file_system_model->index(path);
        if (index.isValid()) {
            file_explorer_view->setCurrentIndex(index);
        }
    }

    update_folder_contents(path);
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

    const QModelIndex new_root = folder_contents_model->setRootPath(directory);
    folder_contents_view->setRootIndex(new_root);
    folder_contents_view->scrollTo(new_root, QAbstractItemView::PositionAtTop);

    for (int col = 0; col < folder_contents_model->columnCount(); ++col) {
        folder_contents_view->resizeColumnToContents(col);
    }
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


void MainApp::perform_analysis()
{
    const std::string directory_path = get_folder_path();
    core_logger->info("Starting analysis for directory '{}'", directory_path);

    run_on_ui([this, directory_path]() {
        if (progress_dialog) {
            progress_dialog->append_text(fmt::format("[SCAN] Exploring {}", directory_path));
        }
    });

    if (stop_analysis.load()) {
        return;
    }

    try {
        const std::vector<CategorizedFile> cleared =
            categorization_service.prune_empty_cached_entries(directory_path);
        if (!cleared.empty()) {
            if (core_logger) {
                core_logger->warn("Cleared {} cached categorization entr{} with empty values for '{}'",
                                  cleared.size(),
                                  cleared.size() == 1 ? "y" : "ies",
                                  directory_path);
                for (const auto& entry : cleared) {
                    core_logger->warn("  - {}", entry.file_name);
                }
            }
            std::string reason =
                "Cached category was empty. The item will be analyzed again.";
            if (!using_local_llm) {
                reason += " Configure your remote API key before analyzing.";
            }
            notify_recategorization_reset(cleared, reason);
        }

        already_categorized_files = categorization_service.load_cached_entries(directory_path);

        if (!already_categorized_files.empty()) {
            run_on_ui([this]() {
                if (progress_dialog) {
                    progress_dialog->append_text("[ARCHIVE] Already categorized highlights:");
                }
            });
        }

        for (const auto& file_entry : already_categorized_files) {
            if (stop_analysis.load()) {
                return;
            }
            const char* symbol = file_entry.type == FileType::Directory ? "DIR" : "FILE";
            const std::string sub = file_entry.subcategory.empty() ? "-" : file_entry.subcategory;
            const std::string message = fmt::format(
                "  - [{}] {} -> {} / {}",
                symbol,
                file_entry.file_name,
                file_entry.category,
                sub);

            run_on_ui([this, message]() {
                if (progress_dialog) {
                    progress_dialog->append_text(message);
                }
            });
        }

        const auto cached_file_names = results_coordinator.extract_file_names(already_categorized_files);
        files_to_categorize = results_coordinator.find_files_to_categorize(directory_path, file_scan_options, cached_file_names);
        core_logger->debug("Found {} item(s) pending categorization in '{}'.",
                           files_to_categorize.size(), directory_path);

        run_on_ui([this]() {
            if (!progress_dialog) {
                return;
            }
            if (!files_to_categorize.empty()) {
                progress_dialog->append_text("[QUEUE] Items waiting for categorization:");
            } else {
                progress_dialog->append_text("[DONE] No files to categorize.");
            }
        });

        for (const auto& file_entry : files_to_categorize) {
            if (stop_analysis.load()) {
                return;
            }
            run_on_ui([this, file_entry]() {
                if (!progress_dialog) {
                    return;
                }
                const char* symbol = file_entry.type == FileType::Directory ? "DIR" : "FILE";
                progress_dialog->append_text(fmt::format("  - [{}] {}", symbol, file_entry.file_name));
            });
        }

        if (stop_analysis.load()) {
            return;
        }

        run_on_ui([this]() {
            if (progress_dialog) {
                progress_dialog->append_text("[PROCESS] Letting the AI do its magic...");
            }
        });

        new_files_with_categories = categorization_service.categorize_entries(
            files_to_categorize,
            using_local_llm,
            stop_analysis,
            [this](const std::string& message) {
                run_on_ui([this, message]() {
                    if (progress_dialog) {
                        progress_dialog->append_text(message);
                    }
                });
            },
            [this](const FileEntry& entry) {
                run_on_ui([this, entry]() {
                    if (progress_dialog) {
                        progress_dialog->append_text(
                            fmt::format("[SORT] {} ({})", entry.file_name,
                                        entry.type == FileType::Directory ? "directory" : "file"));
                    }
                });
            },
            [this](const CategorizedFile& entry, const std::string& reason) {
                notify_recategorization_reset(entry, reason);
            },
            [this]() {
                return make_llm_client();
            });
        core_logger->info("Categorization produced {} new record(s).",
                          new_files_with_categories.size());

        already_categorized_files.insert(
            already_categorized_files.end(),
            new_files_with_categories.begin(),
            new_files_with_categories.end());

        // Consistency pass temporarily disabled
        // if (settings.get_consistency_pass_enabled()) {
        //     run_consistency_pass();
        // }

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
            settings.set_llm_choice(dialog->get_selected_llm_choice());
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


std::unique_ptr<ILLMClient> MainApp::make_llm_client()
{
    if (settings.get_llm_choice() == LLMChoice::Remote) {
        CategorizationSession session;
        return std::make_unique<LLMClient>(session.create_llm_client());
    }

    const char* env_var = settings.get_llm_choice() == LLMChoice::Local_3b
        ? "LOCAL_LLM_3B_DOWNLOAD_URL"
        : "LOCAL_LLM_7B_DOWNLOAD_URL";

    const char* env_url = std::getenv(env_var);
    if (!env_url) {
        throw std::runtime_error("Required environment variable for selected model is not set");
    }

    return std::make_unique<LocalLLMClient>(
        Utils::make_default_path_to_file_from_download_url(env_url));
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
        categorization_dialog = std::make_unique<CategorizationDialog>(&db_manager, show_subcategory, this);
        categorization_dialog->show_results(results);
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
