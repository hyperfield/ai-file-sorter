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
#include <future>
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

#ifdef _WIN32
#include <json/json.h>
#elif __APPLE__
#include <json/json.h>
#else
#include <jsoncpp/json/json.h>
#endif

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

const Json::Value* parse_consistency_response(const std::string& response,
                                              Json::Value& root,
                                              const std::shared_ptr<spdlog::logger>& logger)
{
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream stream(response);
    if (!Json::parseFromStream(reader, stream, &root, &errors)) {
        if (logger) {
            logger->warn("Consistency pass JSON parse failed: {}", errors);
            logger->warn("Consistency pass raw response ({} chars):\n{}", response.size(), response);
        }
        return nullptr;
    }

    if (root.isObject() && root.isMember("harmonized")) {
        const Json::Value& harmonized = root["harmonized"];
        if (harmonized.isArray()) {
            return &harmonized;
        }
    }

    if (root.isArray()) {
        return &root;
    }

    if (logger) {
        logger->warn("Consistency pass response missing 'harmonized' array");
    }
    return nullptr;
}

std::string trim_whitespace(std::string value) {
    const char* ws = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(ws);
    if (start == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(ws);
    return value.substr(start, end - start + 1);
}

struct HarmonizedUpdate {
    std::string id;
    CategorizedFile* target{nullptr};
    std::string category;
    std::string subcategory;
};

std::optional<HarmonizedUpdate> extract_harmonized_update(
    const Json::Value& entry,
    std::unordered_map<std::string, CategorizedFile*>& items_by_key)
{
    if (!entry.isObject()) {
        return std::nullopt;
    }

    const std::string id = entry.get("id", "").asString();
    if (id.empty()) {
        return std::nullopt;
    }

    auto it = items_by_key.find(id);
    if (it == items_by_key.end() || !it->second) {
        return std::nullopt;
    }

    auto trim_or_fallback = [](const Json::Value& parent,
                               const char* key,
                               const std::string& fallback) {
        if (!parent.isMember(key)) {
            return fallback;
        }
        std::string candidate = parent[key].asString();
        if (candidate.empty()) {
            return fallback;
        }
        candidate = trim_whitespace(std::move(candidate));
        return candidate.empty() ? fallback : candidate;
    };

    CategorizedFile* target = it->second;
    std::string category = trim_or_fallback(entry, "category", target->category);
    if (category.empty()) {
        category = target->category;
    }

    std::string subcategory = trim_or_fallback(entry, "subcategory", target->subcategory);
    if (subcategory.empty()) {
        subcategory = category;
    }

    return HarmonizedUpdate{id, target, std::move(category), std::move(subcategory)};
}

void apply_harmonized_update(
    const HarmonizedUpdate& update,
    DatabaseManager& db_manager,
    std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
    const std::function<void(const std::string&)>& progress_sink)
{
    DatabaseManager::ResolvedCategory resolved =
        db_manager.resolve_category(update.category, update.subcategory);

    bool changed = (resolved.category != update.target->category) ||
                   (resolved.subcategory != update.target->subcategory);

    update.target->category = resolved.category;
    update.target->subcategory = resolved.subcategory;
    update.target->taxonomy_id = resolved.taxonomy_id;

    db_manager.insert_or_update_file_with_categorization(
        update.target->file_name,
        update.target->type == FileType::File ? "F" : "D",
        update.target->file_path,
        resolved);

    if (auto new_it = new_items_by_key.find(update.id); new_it != new_items_by_key.end() && new_it->second) {
        new_it->second->category = resolved.category;
        new_it->second->subcategory = resolved.subcategory;
        new_it->second->taxonomy_id = resolved.taxonomy_id;
    }

    if (changed) {
        const std::string message = fmt::format("[CONSISTENCY] {} -> {} / {}",
                                                update.target->file_name,
                                                resolved.category,
                                                resolved.subcategory);
        progress_sink(message);
    }
}

} // namespace


MainApp::MainApp(Settings& settings, QWidget* parent)
    : QMainWindow(parent),
      settings(settings),
      db_manager(settings.get_config_dir()),
      core_logger(Logger::get_logger("core_logger")),
      ui_logger(Logger::get_logger("ui_logger"))
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
        if (!ensure_remote_credentials_available(&credential_error)) {
            if (!credential_error.empty()) {
                show_error_dialog(credential_error);
            } else {
                show_error_dialog("Remote model credentials are missing or invalid. Please configure your API key and try again.");
            }
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
            db_manager.remove_empty_categorizations(directory_path);
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

        // Consistency pass temporarily disabled
        // if (settings.get_consistency_pass_enabled()) {
        //     run_consistency_pass();
        // }

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
        const std::filesystem::path entry_path = Utils::utf8_to_path(entry.full_path);
        const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
        const std::string abbreviated_path = Utils::abbreviate_user_path(entry.full_path);

        DatabaseManager::ResolvedCategory resolved =
            categorize_file(llm, entry.file_name, abbreviated_path, entry.type, report);

        if (resolved.category.empty() || resolved.subcategory.empty()) {
            core_logger->warn("Categorization for '{}' returned empty category/subcategory.", entry.file_name);
            db_manager.remove_file_categorization(dir_path, entry.file_name, entry.type);
            CategorizedFile placeholder{dir_path,
                                        entry.file_name,
                                        entry.type,
                                        resolved.category,
                                        resolved.subcategory,
                                        resolved.taxonomy_id};
            const std::string reason = using_local_llm
                ? "Categorization returned no result. The item will be processed again."
                : "Categorization returned no result. Configure your remote API key and try again.";
            notify_recategorization_reset(placeholder, reason);
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

        const std::string trimmed_category = trim_whitespace(category);
        const std::string trimmed_subcategory = trim_whitespace(subcategory);
        if (trimmed_category.empty() || trimmed_subcategory.empty()) {
            if (core_logger) {
                core_logger->warn("Ignoring cached categorization with empty values for '{}'", item_name);
            }
        } else {
            auto resolved = db_manager.resolve_category(category, subcategory);
            std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
            std::string path_display = item_path.empty() ? "-" : item_path;

            std::string message = fmt::format(
                "[CACHE] {}\n    Category : {}\n    Subcat   : {}\n    Path     : {}",
                item_name, resolved.category, sub, path_display);
            report_progress(message);
            return resolved;
        }
    }

    if (!using_local_llm) {
        const char* env_pc = std::getenv("ENV_PC");
        const char* env_rr = std::getenv("ENV_RR");

        try {
            CryptoManager crypto(env_pc, env_rr);
            const std::string key = crypto.reconstruct();
            if (key.empty()) {
                throw std::runtime_error("Reconstructed API key was empty");
            }
        } catch (const std::exception& ex) {
            std::string err_msg = fmt::format("[CRYPTO] {} ({})", item_name, ex.what());
            report_progress(err_msg);
            core_logger->error("{}", err_msg);
            return DatabaseManager::ResolvedCategory{-1, "", ""};
        }
    }

    try {
        std::string category_subcategory;

        int timeout_seconds = using_local_llm ? 60 : 10;
        const char* timeout_env = std::getenv(
            using_local_llm ? "AI_FILE_SORTER_LOCAL_LLM_TIMEOUT"
                            : "AI_FILE_SORTER_REMOTE_LLM_TIMEOUT");
        if (timeout_env && *timeout_env) {
            try {
                int parsed = std::stoi(timeout_env);
                if (parsed > 0) {
                    timeout_seconds = parsed;
                } else if (core_logger) {
                    core_logger->warn("Ignoring non-positive LLM timeout '{}'", timeout_env);
                }
            } catch (const std::exception& ex) {
                if (core_logger) {
                    core_logger->warn("Failed to parse LLM timeout '{}': {}", timeout_env, ex.what());
                }
            }
        }
        if (core_logger) {
            core_logger->debug("Using {} LLM timeout of {} second(s)",
                               using_local_llm ? "local" : "remote",
                               timeout_seconds);
        }
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

bool MainApp::ensure_remote_credentials_available(std::string* error_message)
{
    if (using_local_llm) {
        return true;
    }

    const char* env_pc = std::getenv("ENV_PC");
    const char* env_rr = std::getenv("ENV_RR");

    try {
        CryptoManager crypto(env_pc, env_rr);
        const std::string key = crypto.reconstruct();
        if (key.empty()) {
            throw std::runtime_error("Reconstructed API key was empty");
        }
        return true;
    } catch (const std::exception& ex) {
        if (core_logger) {
            core_logger->error("Remote LLM credentials unavailable: {}", ex.what());
        }
        if (error_message) {
            *error_message = "Remote model credentials are missing or invalid. "
                             "Please configure your API key and try again.";
        }
        return false;
    }
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


std::string MainApp::make_item_key(const CategorizedFile& item)
{
    std::filesystem::path path(item.file_path);
    path /= item.file_name;
    return path.generic_string();
}


std::string MainApp::build_consistency_prompt(const std::vector<const CategorizedFile*>& chunk,
                                              const std::vector<std::pair<std::string, std::string>>& taxonomy) const
{
    Json::Value taxonomy_json(Json::arrayValue);
    for (const auto& entry : taxonomy) {
        Json::Value obj;
        obj["category"] = entry.first;
        obj["subcategory"] = entry.second;
        taxonomy_json.append(obj);
    }

    Json::Value items(Json::arrayValue);
    for (const auto* item : chunk) {
        if (!item) {
            continue;
        }
        Json::Value obj;
        obj["id"] = make_item_key(*item);
        obj["file"] = item->file_name;
        obj["category"] = item->category;
        obj["subcategory"] = item->subcategory;
        items.append(obj);
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const std::string taxonomy_str = Json::writeString(builder, taxonomy_json);
    const std::string items_str = Json::writeString(builder, items);

    std::ostringstream prompt;
    prompt << "You are a taxonomy normalization assistant.\n";
    prompt << "Your task is to review existing (category, subcategory) assignments for files and make them consistent.\n";
    prompt << "Guidelines:\n";
    prompt << "1. Prefer using the known taxonomy entries when they closely match.\n";
    prompt << "2. Merge near-duplicate labels (e.g. 'Docs' vs 'Documents'), but do not collapse distinct concepts.\n";
    prompt << "3. Preserve the intent of each file. If a category/subcategory already looks appropriate, keep it.\n";
    prompt << "4. Always provide both category and subcategory strings. If a subcategory is not needed, repeat the category.\n";
    prompt << "5. Output JSON only, no prose.\n\n";
    prompt << "Known taxonomy entries (JSON array): " << taxonomy_str << "\n";
    prompt << "Items to harmonize (JSON array): " << items_str << "\n";
    prompt << "Return a JSON object with the following structure: {\"harmonized\": [{\"id\": string, \"category\": string, \"subcategory\": string}, ...]}";
    prompt << "\nMatch each output object to the corresponding input by id and keep the order identical.";

    return prompt.str();
}


void MainApp::apply_consistency_response(const std::string& response,
                                          std::unordered_map<std::string, CategorizedFile*>& items_by_key,
                                          std::unordered_map<std::string, CategorizedFile*>& new_items_by_key)
{
    if (response.empty()) {
        return;
    }

    Json::Value root;
    const Json::Value* harmonized = parse_consistency_response(response, root, core_logger);
    if (!harmonized) {
        return;
    }

    auto progress_sink = [this](const std::string& message) {
        run_on_ui([this, message]() {
            if (progress_dialog) {
                progress_dialog->append_text(message);
            }
        });
    };

    for (const auto& entry : *harmonized) {
        if (auto update = extract_harmonized_update(entry, items_by_key)) {
            apply_harmonized_update(*update, db_manager, new_items_by_key, progress_sink);
        }
    }
}


void MainApp::run_consistency_pass()
{
    if (stop_analysis.load()) {
        return;
    }
    if (already_categorized_files.empty()) {
        return;
    }

    std::unique_ptr<ILLMClient> llm;
    try {
        llm = make_llm_client();
    } catch (const std::exception& ex) {
        if (core_logger) {
            core_logger->warn("Failed to create LLM client for consistency pass: {}", ex.what());
        }
        return;
    }
    if (!llm) {
        return;
    }

    const auto taxonomy = db_manager.get_taxonomy_snapshot(150);

    std::unordered_map<std::string, CategorizedFile*> items_by_key;
    items_by_key.reserve(already_categorized_files.size());
    for (auto& item : already_categorized_files) {
        items_by_key[make_item_key(item)] = &item;
    }

    std::unordered_map<std::string, CategorizedFile*> new_items_by_key;
    new_items_by_key.reserve(new_files_with_categories.size());
    for (auto& item : new_files_with_categories) {
        new_items_by_key[make_item_key(item)] = &item;
    }

    std::vector<const CategorizedFile*> chunk;
    chunk.reserve(10);

    for (size_t index = 0; index < already_categorized_files.size(); ++index) {
        if (stop_analysis.load()) {
            break;
        }

        chunk.push_back(&already_categorized_files[index]);
        bool should_flush = chunk.size() == 10 || index + 1 == already_categorized_files.size();
        if (!should_flush) {
            continue;
        }

        if (core_logger) {
            core_logger->info("[CONSISTENCY] Processing chunk {}-{} of {}", index + 1 - chunk.size() + 1, index + 1, already_categorized_files.size());
            for (const auto* item : chunk) {
                if (!item) continue;
                core_logger->info("  [BEFORE] {} -> {} / {}", item->file_name, item->category, item->subcategory);
            }
        }

        const std::string prompt = build_consistency_prompt(chunk, taxonomy);
        try {
            const std::string response = llm->complete_prompt(prompt, 512);
            apply_consistency_response(response, items_by_key, new_items_by_key);
        } catch (const std::exception& ex) {
            if (core_logger) {
                core_logger->warn("Consistency pass chunk failed: {}", ex.what());
            }
        }

        if (core_logger) {
            for (const auto* item : chunk) {
                if (!item) continue;
                core_logger->info("  [AFTER] {} -> {} / {}", item->file_name, item->category, item->subcategory);
            }
        }

        chunk.clear();
    }
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
    status_is_ready_ = false;
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
