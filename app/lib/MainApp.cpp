#include "MainApp.hpp"

#include "CategorizationSession.hpp"
#include "CryptoManager.hpp"
#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "LLMClient.hpp"
#include "LLMSelectionDialog.hpp"
#include "Logger.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "Updater.hpp"
#include "Utils.hpp"
#include "Types.hpp"

#include <QAction>
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
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QDialog>
#include <QWidget>
#include <QIcon>
#include <QDir>
#include <QStyle>

#include <chrono>
#include <filesystem>
#include <future>
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <LocalLLMClient.hpp>

using namespace std::chrono_literals;

namespace {

std::tuple<std::string, std::string> split_category_subcategory(const std::string& input)
{
    const std::string delimiter = " : ";

    auto pos = input.find(delimiter);
    if (pos == std::string::npos) {
        return {input, ""};
    }

    std::string category = input.substr(0, pos);
    std::string subcategory = input.substr(pos + delimiter.size());

    auto trim = [](std::string value) {
        const char* whitespace = " \t\n\r\f\v";
        size_t start = value.find_first_not_of(whitespace);
        size_t end = value.find_last_not_of(whitespace);
        if (start == std::string::npos || end == std::string::npos) {
            return std::string();
        }
        return value.substr(start, end - start + 1);
    };

    return {trim(category), trim(subcategory)};
}

} // namespace


MainApp::MainApp(Settings& settings, QWidget* parent)
    : QMainWindow(parent),
      settings(settings),
      db_manager(settings.get_config_dir()),
      core_logger(Logger::get_logger("core_logger")),
      ui_logger(Logger::get_logger("ui_logger"))
{
    if (settings.get_llm_choice() != LLMChoice::Remote) {
        using_local_llm = true;
    }

    setup_ui();
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


void MainApp::setup_ui()
{
    setWindowTitle(QStringLiteral("QN AI File Sorter"));
    resize(1000, 800);

    QWidget* central = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    // Path selection row
    auto* path_layout = new QHBoxLayout();
    auto* path_label = new QLabel(tr("Folder:"), central);
    path_entry = new QLineEdit(central);
    browse_button = new QPushButton(tr("Browse…"), central);
    path_layout->addWidget(path_label);
    path_layout->addWidget(path_entry, 1);
    path_layout->addWidget(browse_button);
    main_layout->addLayout(path_layout);

    // Options
    auto* options_layout = new QHBoxLayout();
    use_subcategories_checkbox = new QCheckBox(tr("Use subcategories"), central);
    categorize_files_checkbox = new QCheckBox(tr("Categorize files"), central);
    categorize_directories_checkbox = new QCheckBox(tr("Categorize directories"), central);
    categorize_files_checkbox->setChecked(true);
    options_layout->addWidget(use_subcategories_checkbox);
    options_layout->addWidget(categorize_files_checkbox);
    options_layout->addWidget(categorize_directories_checkbox);
    options_layout->addStretch(1);
    main_layout->addLayout(options_layout);

    // Analyze button
    analyze_button = new QPushButton(tr("Analyze folder"), central);
    QIcon analyze_icon = QIcon::fromTheme(QStringLiteral("sparkle"));
    if (analyze_icon.isNull()) {
        analyze_icon = QIcon::fromTheme(QStringLiteral("applications-education"));
    }
    if (analyze_icon.isNull()) {
        analyze_icon = style()->standardIcon(QStyle::SP_MediaPlay);
    }
    analyze_button->setIcon(analyze_icon);
    analyze_button->setIconSize(QSize(20, 20));
    analyze_button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    main_layout->addWidget(analyze_button);

    // Tree view for quick summary
    tree_model = new QStandardItemModel(0, 5, this);
    tree_model->setHorizontalHeaderLabels({
        tr("File"),
        tr("Type"),
        tr("Category"),
        tr("Subcategory"),
        tr("Status")
    });

    tree_view = new QTreeView(central);
    tree_view->setModel(tree_model);
    tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_view->header()->setSectionResizeMode(QHeaderView::Stretch);
    main_layout->addWidget(tree_view, 1);

    setCentralWidget(central);

    setup_menus();
    statusBar()->showMessage(tr("Ready"));
}


void MainApp::setup_menus()
{
    auto themed_icon = [this](const char* name, QStyle::StandardPixmap fallback) {
        QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
        if (icon.isNull()) {
            icon = style()->standardIcon(fallback);
        }
        return icon;
    };

    QMenu* file_menu = menuBar()->addMenu(tr("&File"));
    QAction* file_quit = file_menu->addAction(themed_icon("application-exit", QStyle::SP_DialogCloseButton), tr("&Quit"));
    file_quit->setShortcut(QKeySequence::Quit);
    connect(file_quit, &QAction::triggered, qApp, &QApplication::quit);

    QMenu* edit_menu = menuBar()->addMenu(tr("&Edit"));
    QAction* copy_action = edit_menu->addAction(themed_icon("edit-copy", QStyle::SP_FileDialogContentsView), tr("&Copy"));
    connect(copy_action, &QAction::triggered, this, [this]() {
        MainAppEditActions::on_copy(path_entry);
    });
    copy_action->setShortcut(QKeySequence::Copy);

    QAction* cut_action = edit_menu->addAction(themed_icon("edit-cut", QStyle::SP_FileDialogDetailedView), tr("Cu&t"));
    connect(cut_action, &QAction::triggered, this, [this]() {
        MainAppEditActions::on_cut(path_entry);
    });
    cut_action->setShortcut(QKeySequence::Cut);

    QAction* paste_action = edit_menu->addAction(themed_icon("edit-paste", QStyle::SP_FileDialogListView), tr("&Paste"));
    connect(paste_action, &QAction::triggered, this, [this]() {
        MainAppEditActions::on_paste(path_entry);
    });
    paste_action->setShortcut(QKeySequence::Paste);

    QAction* delete_action = edit_menu->addAction(themed_icon("edit-delete", QStyle::SP_TrashIcon), tr("&Delete"));
    connect(delete_action, &QAction::triggered, this, [this]() {
        MainAppEditActions::on_delete(path_entry);
    });
    delete_action->setShortcut(QKeySequence::Delete);

    QMenu* view_menu = menuBar()->addMenu(tr("&View"));
    QAction* toggle_explorer = view_menu->addAction(themed_icon("system-file-manager", QStyle::SP_DirOpenIcon), tr("File &Explorer"));
    toggle_explorer->setCheckable(true);
    toggle_explorer->setChecked(settings.get_show_file_explorer());
    connect(toggle_explorer, &QAction::toggled, this, [this](bool checked) {
        if (file_explorer_dock) {
            file_explorer_dock->setVisible(checked);
        }
        settings.set_show_file_explorer(checked);
    });
    file_explorer_menu_action = toggle_explorer;

    QMenu* settings_menu = menuBar()->addMenu(tr("&Settings"));
    QAction* toggle_llm = settings_menu->addAction(themed_icon("preferences-system", QStyle::SP_DialogApplyButton), tr("Select &LLM…"));
    connect(toggle_llm, &QAction::triggered, this, &MainApp::show_llm_selection_dialog);

    QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
    QAction* about_action = help_menu->addAction(themed_icon("help-about", QStyle::SP_MessageBoxInformation), tr("&About"));
    connect(about_action, &QAction::triggered, this, &MainApp::on_about_activate);
}


void MainApp::setup_file_explorer()
{
    file_explorer_dock = new QDockWidget(tr("File Explorer"), this);
    file_explorer_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, file_explorer_dock);

    file_system_model = new QFileSystemModel(file_explorer_dock);
    file_system_model->setRootPath(QDir::homePath());
    file_system_model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

    file_explorer_view = new QTreeView(file_explorer_dock);
    file_explorer_view->setModel(file_system_model);
    file_explorer_view->setRootIndex(file_system_model->index(QDir::homePath()));
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

    file_explorer_dock->setWidget(file_explorer_view);

    const bool show_explorer = settings.get_show_file_explorer();
    if (file_explorer_menu_action) {
        file_explorer_menu_action->setChecked(show_explorer);
    }
    file_explorer_dock->setVisible(show_explorer);
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
    QIcon icon(QStringLiteral(":/net/quicknode/AIFileSorter/images/logo.png"));
    if (!icon.isNull()) {
        setWindowIcon(icon);
    }
}


void MainApp::load_settings()
{
    if (!settings.load()) {
        core_logger->info("Failed to load settings, using defaults.");
    }
    sync_settings_to_ui();
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
}


void MainApp::sync_ui_to_settings()
{
    settings.set_use_subcategories(use_subcategories_checkbox->isChecked());
    settings.set_categorize_files(categorize_files_checkbox->isChecked());
    settings.set_categorize_directories(categorize_directories_checkbox->isChecked());
    settings.set_sort_folder(path_entry->text().toStdString());
    if (file_explorer_menu_action) {
        settings.set_show_file_explorer(file_explorer_menu_action->isChecked());
    }
}


void MainApp::on_analyze_clicked()
{
    if (analyze_thread.joinable()) {
        stop_running_analysis();
        update_analyze_button_state(false);
        statusBar()->showMessage(tr("Analysis cancelled"), 4000);
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
    if (analyzing) {
        analyze_button->setText(tr("Stop analyzing"));
        statusBar()->showMessage(tr("Analyzing…"));
    } else {
        analyze_button->setText(tr("Analyze folder"));
        statusBar()->showMessage(tr("Ready"));
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
        row << new QStandardItem(QString::fromStdString(file.file_name));
        row << new QStandardItem(file.type == FileType::Directory ? tr("Directory") : tr("File"));
        row << new QStandardItem(QString::fromStdString(file.category));
        row << new QStandardItem(QString::fromStdString(file.subcategory));
        row << new QStandardItem(QStringLiteral("Ready"));
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
        already_categorized_files = db_manager.get_categorized_files(directory_path);

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

        const std::unordered_set<std::string> cached_file_names = extract_file_names(already_categorized_files);

        files_to_categorize = find_files_to_categorize(directory_path, cached_file_names);
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

        new_files_with_categories = categorize_files(files_to_categorize);
        core_logger->info("Categorization produced {} new record(s).",
                          new_files_with_categories.size());

        already_categorized_files.insert(
            already_categorized_files.end(),
            new_files_with_categories.begin(),
            new_files_with_categories.end());

        new_files_to_sort = compute_files_to_sort();
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


std::unordered_set<std::string> MainApp::extract_file_names(
    const std::vector<CategorizedFile>& categorized_files)
{
    std::unordered_set<std::string> file_names;
    for (const auto& file : categorized_files) {
        file_names.insert(file.file_name);
    }
    return file_names;
}


std::vector<FileEntry> MainApp::find_files_to_categorize(
    const std::string& directory_path,
    const std::unordered_set<std::string>& cached_files)
{
    std::vector<FileEntry> actual_files =
        dirscanner.get_directory_entries(directory_path, file_scan_options);
    core_logger->debug("Directory '{}' has {} actual item(s); {} cached entry name(s) loaded.",
                       directory_path, actual_files.size(), cached_files.size());

    std::vector<FileEntry> found_files;
    for (const auto& entry : actual_files) {
        if (!cached_files.contains(entry.file_name)) {
            found_files.push_back(entry);
        }
    }

    core_logger->debug("{} item(s) require categorization after cache comparison.", found_files.size());
    return found_files;
}


std::vector<CategorizedFile> MainApp::categorize_files(const std::vector<FileEntry>& files)
{
    std::vector<CategorizedFile> categorized;
    if (files.empty()) {
        return categorized;
    }

    auto llm = make_llm_client();
    if (!llm) {
        throw std::runtime_error("Failed to create LLM client.");
    }

    for (const auto& entry : files) {
        if (stop_analysis.load()) {
            break;
        }

        run_on_ui([this, entry]() {
            if (progress_dialog) {
                progress_dialog->append_text(
                    fmt::format("[SORT] {} ({})", entry.file_name,
                                entry.type == FileType::Directory ? "directory" : "file"));
            }
        });

        if (auto categorized_file = categorize_single_file(*llm, entry)) {
            categorized.push_back(*categorized_file);
        }
    }

    return categorized;
}


std::optional<CategorizedFile> MainApp::categorize_single_file(
    ILLMClient& llm, const FileEntry& entry)
{
    auto report = [this](const std::string& message) {
        run_on_ui([this, message]() {
            if (progress_dialog) {
                progress_dialog->append_text(message);
            }
        });
    };

    try {
        const std::string dir_path = std::filesystem::path(entry.full_path).parent_path().string();
        const std::string abbreviated_path = Utils::abbreviate_user_path(entry.full_path);

        DatabaseManager::ResolvedCategory resolved =
            categorize_file(llm, entry.file_name, abbreviated_path, entry.type, report);

        if (resolved.category.empty() || resolved.subcategory.empty()) {
            core_logger->warn("Categorization for '{}' returned empty category/subcategory.", entry.file_name);
            return std::nullopt;
        }

        core_logger->info("Categorized '{}' as '{} / {}'.", entry.file_name, resolved.category,
                          resolved.subcategory.empty() ? "<none>" : resolved.subcategory);

        return CategorizedFile{dir_path, entry.file_name, entry.type,
                               resolved.category, resolved.subcategory, resolved.taxonomy_id};
    } catch (const std::exception& ex) {
        const std::string error_message = fmt::format("Error categorizing file '{}': {}", entry.file_name, ex.what());
        run_on_ui([this, error_message]() {
            show_error_dialog(error_message);
        });
        core_logger->error("{}", error_message);
        return std::nullopt;
    }
}


DatabaseManager::ResolvedCategory
MainApp::categorize_file(ILLMClient& llm, const std::string& item_name,
                         const std::string& item_path,
                         const FileType file_type,
                         const std::function<void(const std::string&)>& report_progress)
{
    auto categorization = db_manager.get_categorization_from_db(item_name, file_type);
    if (categorization.size() >= 2) {
        const std::string& category = categorization[0];
        const std::string& subcategory = categorization[1];

        auto resolved = db_manager.resolve_category(category, subcategory);
        std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
        std::string path_display = item_path.empty() ? "-" : item_path;

        std::string message = fmt::format(
            "[CACHE] {}\n    Category : {}\n    Subcat   : {}\n    Path     : {}",
            item_name, resolved.category, sub, path_display);
        report_progress(message);
        return resolved;
    }

    if (!using_local_llm) {
        const char* env_pc = std::getenv("ENV_PC");
        const char* env_rr = std::getenv("ENV_RR");

        std::string key;
        try {
            CryptoManager crypto(env_pc, env_rr);
            key = crypto.reconstruct();
        } catch (const std::exception& ex) {
            std::string err_msg = fmt::format("[CRYPTO] {} ({})", item_name, ex.what());
            report_progress(err_msg);
            core_logger->error("{}", err_msg);
            return DatabaseManager::ResolvedCategory{-1, "", ""};
        }
    }

    try {
        std::string category_subcategory;

        const int timeout_seconds = using_local_llm ? 60 : 10;
        category_subcategory = categorize_with_timeout(
            llm, item_name, item_path, file_type, timeout_seconds);

        auto [category, subcategory] = split_category_subcategory(category_subcategory);
        auto resolved = db_manager.resolve_category(category, subcategory);

        std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
        std::string path_display = item_path.empty() ? "-" : item_path;

        std::string message = fmt::format(
            "[AI] {}\n    Category : {}\n    Subcat   : {}\n    Path     : {}",
            item_name, resolved.category, sub, path_display);
        report_progress(message);

        return resolved;
    } catch (const std::exception& ex) {
        std::string err_msg = fmt::format("[LLM-ERROR] {} ({})", item_name, ex.what());
        report_progress(err_msg);
        core_logger->error("LLM error while categorizing '{}': {}", item_name, ex.what());
        throw;
    }
}


std::string MainApp::categorize_with_timeout(
    ILLMClient& llm, const std::string& item_name,
    const std::string& item_path,
    const FileType file_type,
    int timeout_seconds)
{
    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread([&llm, &promise, item_name, item_path, file_type]() mutable {
        try {
            promise.set_value(llm.categorize_file(item_name, item_path, file_type));
        } catch (...) {
            try {
                promise.set_exception(std::current_exception());
            } catch (...) {
                // no-op
            }
        }
    }).detach();

    if (future.wait_for(std::chrono::seconds(timeout_seconds)) == std::future_status::timeout) {
        throw std::runtime_error("Timed out waiting for LLM response");
    }

    return future.get();
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


void MainApp::request_stop_analysis()
{
    stop_analysis = true;
    statusBar()->showMessage(tr("Cancelling analysis…"), 4000);
}


std::vector<FileEntry> MainApp::get_actual_files(const std::string& directory_path)
{
    core_logger->info("Getting actual files from directory {}", directory_path);

    std::vector<FileEntry> actual_files =
        dirscanner.get_directory_entries(directory_path, FileScanOptions::Files | FileScanOptions::Directories);

    core_logger->info("Actual files found: {}", static_cast<int>(actual_files.size()));
    for (const auto& entry : actual_files) {
        core_logger->info("File: {}, Path: {}", entry.file_name, entry.full_path);
    }

    return actual_files;
}


std::vector<CategorizedFile> MainApp::compute_files_to_sort()
{
    std::vector<CategorizedFile> files_to_sort;

    const std::vector<FileEntry> actual_files =
        dirscanner.get_directory_entries(get_folder_path(), file_scan_options);
    core_logger->debug("Computing files to sort. {} entries currently in directory.", actual_files.size());

    for (const auto& entry : actual_files) {
        const auto it = std::find_if(
            already_categorized_files.begin(),
            already_categorized_files.end(),
            [&entry](const CategorizedFile& categorized_file) {
                return categorized_file.file_name == entry.file_name
                       && categorized_file.type == entry.type;
            });

        if (it != already_categorized_files.end()) {
            files_to_sort.push_back(*it);
        }
    }

    core_logger->info("{} file(s) ready for move after reconciliation.", files_to_sort.size());
    return files_to_sort;
}


std::string MainApp::get_folder_path() const
{
    return path_entry->text().toStdString();
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


void MainApp::closeEvent(QCloseEvent* event)
{
    stop_running_analysis();
    save_settings();
    QMainWindow::closeEvent(event);
}
#include <exception>
