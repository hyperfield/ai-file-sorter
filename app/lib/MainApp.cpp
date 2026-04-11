#include "MainApp.hpp"
#include "AnalysisCoordinator.hpp"
#include "AppInfo.hpp"

#include "CategorizationSession.hpp"
#include "DialogUtils.hpp"
#include "ErrorMessages.hpp"
#include "LLMClient.hpp"
#include "GeminiClient.hpp"
#include "LocalFsProvider.hpp"
#include "LLMSelectionDialog.hpp"
#include "Logger.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "MainWindowStateBinder.hpp"
#include "Updater.hpp"
#include "TranslationManager.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"
#include "Types.hpp"
#include "CategoryLanguage.hpp"
#include "MainAppUiBuilder.hpp"
#include "SuitabilityBenchmarkDialog.hpp"
#include "UiTranslator.hpp"
#include "UpdaterBuildConfig.hpp"
#include "SupportCodeManager.hpp"
#include "StoragePluginDialog.hpp"
#include "StoragePluginManager.hpp"
#include "WhitelistManagerDialog.hpp"
#include "UndoManager.hpp"
#include "ggml-backend.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
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
#include <QInputDialog>
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
#include <QThread>
#include <QTimer>
#include <QtGlobal>

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
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <LocalLLMClient.hpp>

using namespace std::chrono_literals;

namespace {

std::string trim_ws_copy(const std::string& value) {
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return std::string();
    }
    const auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string expand_user_path(const std::string& value) {
    if (value.empty() || value.front() != '~') {
        return value;
    }
    QString home = QDir::homePath();
    if (home.isEmpty()) {
        return value;
    }
    if (value.size() == 1) {
        return home.toUtf8().toStdString();
    }
    const char next = value[1];
    if (next == '/' || next == '\\') {
        QString expanded = home + QString::fromUtf8(value.c_str() + 1);
        return expanded.toUtf8().toStdString();
    }
    return value;
}

std::string normalize_directory_path(const std::string& value) {
    const std::string trimmed = trim_ws_copy(value);
    if (trimmed.empty()) {
        return trimmed;
    }
    const std::string expanded = expand_user_path(trimmed);
    const std::filesystem::path fs_path = Utils::utf8_to_path(expanded).lexically_normal();
    std::string normalized = Utils::path_to_utf8(fs_path);
    if (fs_path.has_relative_path()) {
        while (normalized.size() > 1 &&
               (normalized.back() == '/' || normalized.back() == '\\')) {
            normalized.pop_back();
        }
    }
    return normalized;
}

void schedule_next_support_prompt(Settings& settings, int total_files) {
    settings.set_next_support_prompt_threshold(total_files + 50);
    settings.save();
}

void maybe_show_support_prompt(Settings& settings,
                               bool& prompt_active,
                               std::function<MainApp::SupportPromptResult(int)> show_prompt) {
    if (prompt_active) {
        return;
    }

    if (SupportCodeManager(Utils::utf8_to_path(settings.get_config_dir())).is_prompt_permanently_disabled()) {
        return;
    }

    const int total = settings.get_total_categorized_files();
    int threshold = settings.get_next_support_prompt_threshold();
    if (threshold <= 0) {
        const int base = std::max(total, 0);
        threshold = ((base / 50) + 1) * 50;
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

    if (result == MainApp::SupportPromptResult::Support) {
        return;
    }

    schedule_next_support_prompt(settings, total);
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

QString display_name_for_provider_id(const std::string& provider_id)
{
    if (provider_id == "onedrive") {
        return QStringLiteral("OneDrive");
    }
    if (provider_id == "dropbox") {
        return QStringLiteral("Dropbox");
    }
    if (provider_id == "pcloud") {
        return QStringLiteral("pCloud");
    }
    return QString::fromStdString(provider_id);
}

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool case_insensitive_contains(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) {
        return true;
    }

    const auto it = std::search(
        haystack.begin(),
        haystack.end(),
        needle.begin(),
        needle.end(),
        [](char lhs, char rhs) {
            return std::tolower(static_cast<unsigned char>(lhs)) ==
                   std::tolower(static_cast<unsigned char>(rhs));
        });
    return it != haystack.end();
}

void load_status_ggml_backends_once()
{
    static bool loaded = false;
    if (loaded) {
        return;
    }

    const char* ggml_dir = std::getenv("AI_FILE_SORTER_GGML_DIR");
    if (ggml_dir && *ggml_dir) {
        ggml_backend_load_all_from_path(ggml_dir);
    } else {
        ggml_backend_load_all();
    }
    loaded = true;
}

std::optional<std::string> detect_status_blas_backend_label()
{
    load_status_ggml_backends_once();

    const size_t device_count = ggml_backend_dev_count();
    for (size_t i = 0; i < device_count; ++i) {
        auto* device = ggml_backend_dev_get(i);
        if (!device) {
            continue;
        }

        const auto type = ggml_backend_dev_type(device);
        if (type != GGML_BACKEND_DEVICE_TYPE_ACCEL &&
            type != GGML_BACKEND_DEVICE_TYPE_CPU) {
            continue;
        }

        const char* dev_name = ggml_backend_dev_name(device);
        const char* dev_desc = ggml_backend_dev_description(device);
        auto* reg = ggml_backend_dev_backend_reg(device);
        const char* reg_name = reg ? ggml_backend_reg_name(reg) : nullptr;

        const auto matches = [&](std::string_view needle) {
            return case_insensitive_contains(dev_name ? dev_name : "", needle) ||
                   case_insensitive_contains(dev_desc ? dev_desc : "", needle) ||
                   case_insensitive_contains(reg_name ? reg_name : "", needle);
        };

        if (matches("openblas")) {
            return std::string("OpenBLAS");
        }
        if (matches("accelerate")) {
            return std::string("Accelerate");
        }
        if (matches("mkl")) {
            return std::string("MKL");
        }
        if (matches("blis")) {
            return std::string("BLIS");
        }
        if (matches("blas")) {
            return std::string("BLAS");
        }
    }

    return std::nullopt;
}

std::string detect_loaded_backend_key()
{
    const auto read_env = [](const char* name) -> std::string {
        const char* value = std::getenv(name);
        if (!value || !*value) {
            return {};
        }
        return to_lower_copy(trim_ws_copy(value));
    };

    if (std::string backend = read_env("AI_FILE_SORTER_GPU_BACKEND"); !backend.empty()) {
        return backend;
    }
    if (std::string device = read_env("LLAMA_ARG_DEVICE"); !device.empty()) {
        return device;
    }

    if (const char* ggml_dir = std::getenv("AI_FILE_SORTER_GGML_DIR");
        ggml_dir && *ggml_dir) {
        const std::string normalized = to_lower_copy(ggml_dir);
        if (normalized.find("vulkan") != std::string::npos) {
            return "vulkan";
        }
        if (normalized.find("cuda") != std::string::npos) {
            return "cuda";
        }
        if (normalized.find("metal") != std::string::npos ||
            normalized.find("mtl") != std::string::npos) {
            return "metal";
        }
        if (normalized.find("cpu") != std::string::npos ||
            normalized.find("wocuda") != std::string::npos) {
            return "cpu";
        }
    }

    const char* disable_cuda = std::getenv("GGML_DISABLE_CUDA");
    if (disable_cuda && disable_cuda[0] != '\0' && disable_cuda[0] != '0') {
        return "cpu";
    }

    return "cpu";
}

QString backend_display_name(std::string_view backend_key)
{
    if (backend_key == "cuda") {
        return QStringLiteral("CUDA");
    }
    if (backend_key == "vulkan") {
        return QStringLiteral("Vulkan");
    }
    if (backend_key == "metal" || backend_key == "mtl") {
        return QStringLiteral("Metal");
    }
    return QStringLiteral("CPU");
}

QString display_name_for_detection_source(const std::string& detection_source)
{
    if (detection_source == "windows_sync_root") {
        return QStringLiteral("Windows sync-root detection");
    }
    if (detection_source == "path_heuristic") {
        return QStringLiteral("path matching");
    }
    if (detection_source == "default_fallback") {
        return QStringLiteral("default fallback");
    }
    return QString::fromStdString(detection_source);
}

} // namespace

MainApp::MainApp(Settings& settings, bool development_mode, QWidget* parent)
    : QMainWindow(parent),
      settings(settings),
      db_manager(settings.get_config_dir()),
      core_logger(Logger::get_logger("core_logger")),
      ui_logger(Logger::get_logger("ui_logger")),
      whitelist_store(settings.get_config_dir()),
      storage_plugin_manager_(std::make_shared<StoragePluginManager>(settings.get_config_dir())),
      storage_plugin_loader_(StoragePluginManager::manifest_directory_for_config_dir(settings.get_config_dir())),
      categorization_service(settings, db_manager, core_logger),
      consistency_pass_service(db_manager, core_logger),
      storage_provider_registry_(),
      active_storage_provider_(std::make_shared<LocalFsProvider>()),
      results_coordinator(*active_storage_provider_),
      undo_manager_(settings.get_config_dir() + "/undo", &storage_provider_registry_),
      development_mode_(development_mode),
      development_prompt_logging_enabled_(development_mode ? settings.get_development_prompt_logging() : false),
      main_window_state_binder_(std::make_unique<MainWindowStateBinder>(*this))
{
    rebuild_storage_provider_registry();
    TranslationManager::instance().initialize_for_app(qApp, settings.get_language());
    initialize_whitelists();

    using_local_llm = !is_remote_choice(settings.get_llm_choice());

    MainAppUiBuilder ui_builder;
    ui_builder.build(*this);
    backend_status_label = new QLabel(this);
    backend_status_label->setObjectName(QStringLiteral("backendStatusLabel"));
    backend_status_label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    statusBar()->addPermanentWidget(backend_status_label);
    ui_translator_ = std::make_unique<UiTranslator>(ui_builder.build_translator_dependencies(*this));
    retranslate_ui();
    setup_file_explorer();
    connect_signals();
    connect_edit_actions();
#if !defined(AI_FILE_SORTER_TEST_BUILD)
    start_updater();
#endif
    load_settings();
    refresh_backend_status_label();
    set_app_icon();
}


MainApp::~MainApp() = default;


void MainApp::run()
{
    show();
#if !defined(AI_FILE_SORTER_TEST_BUILD)
    maybe_show_suitability_benchmark();
#endif
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
    main_window_state_binder_->connect_checkbox_signals();
}

void MainApp::connect_whitelist_signals()
{
    main_window_state_binder_->connect_whitelist_signals();
}


void MainApp::connect_edit_actions()
{
    path_entry->setContextMenuPolicy(Qt::DefaultContextMenu);
}


void MainApp::start_updater()
{
    if (!UpdaterBuildConfig::update_checks_enabled()) {
        return;
    }
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
    main_window_state_binder_->sync_settings_to_ui();

    if (ui_translator_) {
        ui_translator_->update_language_checks();
    }
}

void MainApp::restore_tree_settings()
{
    main_window_state_binder_->restore_tree_settings();
}

void MainApp::restore_sort_folder_state()
{
    main_window_state_binder_->restore_sort_folder_state();
}

void MainApp::restore_file_scan_options()
{
    main_window_state_binder_->restore_file_scan_options();
}

void MainApp::restore_file_explorer_visibility()
{
    main_window_state_binder_->restore_file_explorer_visibility();
}

void MainApp::restore_development_preferences()
{
    main_window_state_binder_->restore_development_preferences();
}


void MainApp::sync_ui_to_settings()
{
    main_window_state_binder_->sync_ui_to_settings();
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
    refresh_backend_status_label();
}

QString MainApp::current_backend_status_text() const
{
    const LLMChoice choice = settings.get_llm_choice();
    switch (choice) {
        case LLMChoice::Remote_OpenAI:
            return tr("Loaded backend: OpenAI API");
        case LLMChoice::Remote_Gemini:
            return tr("Loaded backend: Gemini API");
        case LLMChoice::Remote_Custom:
            return tr("Loaded backend: Custom API");
        default:
            break;
    }

    if (!using_local_llm) {
        return tr("Loaded backend: Remote API");
    }

    const QString cpu_backend = QString::fromStdString(
        detect_status_blas_backend_label().value_or(std::string("CPU")));
    const std::string backend_key = detect_loaded_backend_key();
    const QString backend_name = backend_display_name(backend_key);

    if (backend_key == "cuda" || backend_key == "vulkan" ||
        backend_key == "metal" || backend_key == "mtl") {
        return tr("Loaded GPU backend: %1 with %2").arg(backend_name, cpu_backend);
    }

    if (cpu_backend.compare(QStringLiteral("CPU"), Qt::CaseInsensitive) == 0) {
        return tr("Loaded CPU backend: CPU");
    }
    return tr("Loaded CPU backend: CPU with %1").arg(cpu_backend);
}

void MainApp::refresh_backend_status_label()
{
    if (!backend_status_label) {
        return;
    }
    backend_status_label->setText(current_backend_status_text());
    backend_status_label->setToolTip(current_backend_status_text());
}

void MainApp::schedule_backend_status_label_refresh()
{
    run_on_ui([this]() {
        refresh_backend_status_label();
    });
}

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
    text_cpu_fallback_choice_.reset();
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
    refresh_active_storage_provider(to_utf8(path), user_initiated);

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
    const bool desired = settings.get_use_consistency_hints();
    const bool recursive = settings.get_include_subdirectories();
    if (!db_manager.has_categorization_style_conflict(folder_path, desired, recursive)) {
        return true;
    }

    const auto style_label = [](bool value) -> QString {
        return value ? tr("More consistent") : tr("More refined");
    };

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Recategorize folder?"));
    box.setText(tr("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?")
                    .arg(style_label(!desired), style_label(desired)));
    QPushButton* recategorize_button = box.addButton(tr("Recategorize"), QMessageBox::AcceptRole);
    box.addButton(tr("Keep existing"), QMessageBox::RejectRole);
    QPushButton* cancel_button = box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == cancel_button) {
        return false;
    }

    if (box.clickedButton() == recategorize_button) {
        if (!db_manager.clear_directory_categorizations(folder_path, recursive)) {
            show_error_dialog(tr("Failed to reset cached categorization for this folder.").toStdString());
            return false;
        }
    }

    return true;
}


void MainApp::ensure_one_checkbox_active(QCheckBox* changed_checkbox)
{
    main_window_state_binder_->ensure_one_checkbox_active(changed_checkbox);
}


void MainApp::update_file_scan_option(FileScanOptions option, bool enabled)
{
    main_window_state_binder_->update_file_scan_option(option, enabled);
}

FileScanOptions MainApp::effective_scan_options() const
{
    return main_window_state_binder_->effective_scan_options();
}

bool MainApp::visual_llm_files_available() const
{
    return main_window_state_binder_->visual_llm_files_available();
}

void MainApp::update_image_analysis_controls()
{
    main_window_state_binder_->update_image_analysis_controls();
}

void MainApp::update_image_only_controls()
{
    main_window_state_binder_->update_image_only_controls();
}

void MainApp::update_document_analysis_controls()
{
    main_window_state_binder_->update_document_analysis_controls();
}

void MainApp::run_llm_selection_dialog_for_visual()
{
    main_window_state_binder_->run_llm_selection_dialog_for_visual();
}

void MainApp::handle_image_analysis_toggle(bool checked)
{
    main_window_state_binder_->handle_image_analysis_toggle(checked);
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

    refresh_active_storage_provider(to_utf8(directory), false);

    const bool previous_flag = suppress_folder_view_sync_;
    suppress_folder_view_sync_ = true;

    const QModelIndex new_root = folder_contents_model->setRootPath(directory);
    folder_contents_view->setRootIndex(new_root);
    folder_contents_view->scrollTo(new_root, QAbstractItemView::PositionAtTop);

    folder_contents_view->resizeColumnToContents(0);

    suppress_folder_view_sync_ = previous_flag;
}

void MainApp::rebuild_storage_provider_registry()
{
    storage_provider_registry_.clear();

    auto local_provider = std::make_shared<LocalFsProvider>();
    storage_provider_registry_.register_builtin(local_provider);
    for (auto& provider : storage_plugin_loader_.create_detection_providers()) {
        storage_provider_registry_.register_builtin(std::move(provider));
    }

    if (storage_plugin_manager_) {
        for (auto& provider : storage_plugin_loader_.create_providers_for_installed_plugins(
                 storage_plugin_manager_->installed_plugin_ids())) {
            storage_provider_registry_.register_builtin(std::move(provider));
        }
    }

    active_storage_provider_ = local_provider;
    results_coordinator.set_storage_provider(*active_storage_provider_);
}

void MainApp::refresh_active_storage_provider(const std::string& directory_path,
                                              bool allow_support_prompt)
{
    auto detection = storage_provider_registry_.detect(directory_path);
    const auto support = resolve_storage_support(detection);
    if (allow_support_prompt) {
        if (support.state == StorageSupportState::DetectedButPluginNotInstalled) {
            if (maybe_install_storage_support(detection, directory_path)) {
                detection = storage_provider_registry_.detect(directory_path);
            }
        } else if (support.state == StorageSupportState::DetectedButNoPluginExists) {
            maybe_warn_about_storage_detection(detection, directory_path);
        }
    }

    const auto resolved = storage_provider_registry_.resolve_for(directory_path);
    if (!resolved || resolved == active_storage_provider_) {
        if (allow_support_prompt) {
            maybe_notify_storage_provider_switch(detection, directory_path);
        }
        return;
    }

    active_storage_provider_ = resolved;
    results_coordinator.set_storage_provider(*active_storage_provider_);

    if (core_logger) {
        if (!detection.detection_source.empty()) {
            core_logger->info("Selected storage provider '{}' for '{}' using {}",
                              active_storage_provider_->id(),
                              directory_path,
                              detection.detection_source);
        } else {
            core_logger->info("Selected storage provider '{}' for '{}'",
                              active_storage_provider_->id(),
                              directory_path);
        }
    }

    if (allow_support_prompt) {
        maybe_notify_storage_provider_switch(detection, directory_path);
    }
}

MainApp::StorageSupportResolution MainApp::resolve_storage_support(
    const StorageProviderDetection& detection) const
{
    StorageSupportResolution resolution;
    if (!detection.matched || detection.provider_id.empty() || detection.provider_id == "local_fs") {
        return resolution;
    }

    std::optional<StoragePluginManifest> plugin;
    if (storage_plugin_manager_) {
        plugin = storage_plugin_manager_->find_plugin_for_provider(detection.provider_id);
    }
    if (!plugin.has_value()) {
        plugin = storage_plugin_loader_.find_plugin_for_provider(detection.provider_id);
    }
    resolution.plugin = plugin;

    if (plugin.has_value()) {
        resolution.plugin_supported =
            storage_plugin_manager_ ? storage_plugin_manager_->supports_plugin(plugin->id)
                                    : storage_plugin_loader_.supports_plugin(*plugin);
        resolution.plugin_installed =
            storage_plugin_manager_ && resolution.plugin_supported &&
            storage_plugin_manager_->is_installed(plugin->id);
    }

    if (resolution.plugin_supported && resolution.plugin_installed) {
        resolution.state = StorageSupportState::DetectedAndSupportedViaPlugin;
    } else if (detection.needs_additional_support && resolution.plugin_supported) {
        resolution.state = StorageSupportState::DetectedButPluginNotInstalled;
    } else if (detection.needs_additional_support) {
        resolution.state = StorageSupportState::DetectedButNoPluginExists;
    }

    return resolution;
}

bool MainApp::maybe_install_storage_support(const StorageProviderDetection& detection,
                                            const std::string& directory_path)
{
    if (!storage_plugin_manager_ || !detection.matched || !detection.needs_additional_support) {
        return false;
    }

    const auto support = resolve_storage_support(detection);
    if (support.state != StorageSupportState::DetectedButPluginNotInstalled ||
        !support.plugin.has_value()) {
        return false;
    }
    const auto& plugin = *support.plugin;

    const std::string prompt_key = plugin.id + "|" + directory_path;
    if (prompt_key == last_storage_support_warning_key_) {
        return false;
    }
    last_storage_support_warning_key_ = prompt_key;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Install Compatibility Support"));
    box.setText(
        tr("Detected a %1 folder.")
            .arg(display_name_for_provider_id(detection.provider_id)));
    box.setInformativeText(
        tr("Install the \"%1\" plugin mode now to enable provider-specific compatibility mode for this folder.")
            .arg(QString::fromStdString(plugin.name)));
    if (!detection.detection_source.empty()) {
        box.setDetailedText(
            tr("Detection source: %1")
                .arg(display_name_for_detection_source(detection.detection_source)));
    }

    auto* install_button = box.addButton(
        tr("Install the %1 plugin mode").arg(QString::fromStdString(plugin.name)),
        QMessageBox::AcceptRole);
    box.setDefaultButton(install_button);
    box.exec();

    if (box.clickedButton() != install_button) {
        return false;
    }

    std::string error;
    if (!storage_plugin_manager_->install(plugin.id, &error)) {
        QMessageBox::warning(this,
                             tr("Install failed"),
                             error.empty()
                                 ? tr("Failed to install compatibility support.")
                                 : QString::fromStdString(error));
        return false;
    }

    rebuild_storage_provider_registry();
    QMessageBox::information(
        this,
        tr("Compatibility Support Installed"),
        tr("Installed \"%1\". The app will now switch to compatibility mode for detected cloud folders.")
            .arg(QString::fromStdString(plugin.name)));
    return true;
}

void MainApp::maybe_warn_about_storage_detection(const StorageProviderDetection& detection,
                                                 const std::string& directory_path)
{
    if (!detection.matched || !detection.needs_additional_support) {
        return;
    }

    const auto support = resolve_storage_support(detection);
    if (support.state != StorageSupportState::DetectedButNoPluginExists) {
        return;
    }

    const std::string warning_key = detection.provider_id + "|" + directory_path;
    if (warning_key == last_storage_support_warning_key_) {
        return;
    }
    last_storage_support_warning_key_ = warning_key;

    const QString provider_name = display_name_for_provider_id(detection.provider_id);
    const QString title = tr("Native Plugin Support Unavailable");
    QString message;
    if (support.plugin.has_value() && !support.plugin_supported) {
        message = tr("A %1 folder has been detected, but the \"%2\" plugin mode is not available on this build. The app will continue in local filesystem mode.")
                      .arg(provider_name, QString::fromStdString(support.plugin->name));
    } else {
        message = tr("A %1 folder has been detected. Sorting on it is not currently supported in native mode via a plugin. The app will continue in local filesystem mode.")
                      .arg(provider_name);
    }
    if (!detection.message.empty()) {
        message += tr("\n\n%1").arg(QString::fromStdString(detection.message));
    }

    if (!detection.detection_source.empty()) {
        QMessageBox box(QMessageBox::Warning,
                        title,
                        message,
                        QMessageBox::Ok,
                        this);
        box.setDetailedText(
            tr("Detection source: %1")
                .arg(display_name_for_detection_source(detection.detection_source)));
        box.exec();
        return;
    }

    QMessageBox::warning(this, title, message);
}

void MainApp::maybe_notify_storage_provider_switch(const StorageProviderDetection& detection,
                                                   const std::string& directory_path)
{
    const auto support = resolve_storage_support(detection);
    if (support.state != StorageSupportState::DetectedAndSupportedViaPlugin ||
        !active_storage_provider_ || active_storage_provider_->id() == "local_fs" ||
        active_storage_provider_->id() != detection.provider_id) {
        return;
    }

    const std::string notice_key = detection.provider_id + "|" + directory_path;
    if (notice_key == last_storage_provider_notice_key_) {
        return;
    }
    last_storage_provider_notice_key_ = notice_key;

    QMessageBox::information(
        this,
        tr("Compatibility Mode Active"),
        detection.detection_source.empty()
            ? tr("Detected a supported cloud folder. The app switched to %1 compatibility mode.")
                  .arg(display_name_for_provider_id(detection.provider_id))
            : tr("Detected a supported cloud folder using %1. The app switched to %2 compatibility mode.")
                  .arg(display_name_for_detection_source(detection.detection_source),
                       display_name_for_provider_id(detection.provider_id)));
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

void MainApp::show_storage_plugin_dialog()
{
    if (!storage_plugin_manager_) {
        return;
    }

    StoragePluginDialog dialog(storage_plugin_manager_, this);
    dialog.exec();

    rebuild_storage_provider_registry();

    const std::string folder_path = get_folder_path();
    if (!folder_path.empty()) {
        refresh_active_storage_provider(folder_path, false);
    }
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
    box.setWindowTitle(QObject::tr("Support %1").arg(app_display_name()));

    const QString headline = tr("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you.")
                                 .arg(total_files);
    const QString details = tr("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                               "If the app saves you time or brings value, please consider supporting it so it can keep improving.");
    const QString code_note = tr("Already donated? Click \"I have already donated\" to enter your donation code and permanently disable this reminder.");

    box.setText(headline);
    box.setInformativeText(details + QStringLiteral("\n\n") + code_note);

    auto* support_btn = box.addButton(tr("Donate to permanently hide the donation dialog"), QMessageBox::ActionRole);
    auto* later_btn = box.addButton(tr("I'm not yet sure"), QMessageBox::ActionRole);
    auto* donated_btn = box.addButton(tr("I have already donated"), QMessageBox::ActionRole);

    const auto apply_button_style = [](QAbstractButton* button,
                                       const QString& background,
                                       const QString& hover,
                                       const QString& text_color,
                                       int font_weight,
                                       const QString& border) {
        if (!button) {
            return;
        }
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  padding: 6px 18px;"
            "  border: %5;"
            "  border-radius: 14px;"
            "  font-weight: %3;"
            "}"
            "QPushButton:hover {"
            "  background-color: %4;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %4;"
            "  opacity: 0.9;"
            "}"
        ).arg(background,
              text_color,
              QString::number(font_weight),
              hover,
              border));
    };

    apply_button_style(support_btn,
                       QStringLiteral("#007aff"),
                       QStringLiteral("#005ec7"),
                       QStringLiteral("white"),
                       800,
                       QStringLiteral("2px solid #005ec7"));
    const QString neutral_bg = QStringLiteral("#bdc3c7");
    const QString neutral_hover = QStringLiteral("#95a5a6");
    apply_button_style(later_btn,
                       neutral_bg,
                       neutral_hover,
                       QStringLiteral("#1f1f1f"),
                       500,
                       QStringLiteral("none"));
    apply_button_style(donated_btn,
                       neutral_bg,
                       neutral_hover,
                       QStringLiteral("#1f1f1f"),
                       500,
                       QStringLiteral("none"));

    if (auto* button_box = box.findChild<QDialogButtonBox*>()) {
        button_box->setCenterButtons(true);
        button_box->setLayoutDirection(Qt::LeftToRight);
        if (auto* layout = qobject_cast<QBoxLayout*>(button_box->layout())) {
            int insert_index = layout->count();
            for (int i = 0; i < layout->count(); ++i) {
                if (auto* item = layout->itemAt(i); item && item->widget() &&
                    qobject_cast<QAbstractButton*>(item->widget())) {
                    insert_index = i;
                    break;
                }
            }

            layout->removeWidget(support_btn);
            layout->removeWidget(later_btn);
            layout->removeWidget(donated_btn);

            layout->insertWidget(insert_index++, support_btn);
            layout->insertWidget(insert_index++, donated_btn);
            layout->insertWidget(insert_index, later_btn);
        }
    }

    support_btn->setAutoDefault(true);
    support_btn->setDefault(true);
    later_btn->setAutoDefault(false);
    donated_btn->setAutoDefault(false);
    support_btn->setFocus();
    box.setDefaultButton(support_btn);
    box.exec();

    const QAbstractButton* clicked = box.clickedButton();
    auto prompt_for_donation_code = [this]() -> SupportPromptResult {
        SupportCodeManager support_codes(Utils::utf8_to_path(settings.get_config_dir()));
        while (true) {
            bool accepted = false;
            const QString code = QInputDialog::getText(
                this,
                tr("Donation code"),
                tr("Enter the donation code generated after your donation.\n"
                   "A valid code will permanently hide the donation dialog."),
                QLineEdit::Normal,
                QString(),
                &accepted);
            if (!accepted) {
                return SupportPromptResult::NotSure;
            }

            if (support_codes.redeem_code(to_utf8(code))) {
                return SupportPromptResult::Support;
            }

            QMessageBox::warning(
                this,
                tr("Invalid donation code"),
                tr("The donation code is invalid. Please try again or press Cancel."));
        }
    };

    if (clicked == support_btn) {
        if (!MainAppHelpActions::open_support_page()) {
            QMessageBox::information(
                this,
                tr("Open donation page"),
                tr("Could not open your browser automatically.\nPlease open this link manually:\n%1")
                    .arg(MainAppHelpActions::support_page_url()));
        }
        return prompt_for_donation_code();
    }
    if (clicked == donated_btn) {
        return prompt_for_donation_code();
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

void MainApp::configure_progress_stages(const std::vector<CategorizationProgressDialog::StagePlan>& stages)
{
    run_on_ui_blocking([this, stages]() {
        if (progress_dialog) {
            progress_dialog->configure_stages(stages);
        }
    });
}

void MainApp::set_progress_stage_items(CategorizationProgressDialog::StageId stage_id,
                                       const std::vector<FileEntry>& items)
{
    run_on_ui_blocking([this, stage_id, items]() {
        if (progress_dialog) {
            progress_dialog->set_stage_items(stage_id, items);
        }
    });
}

void MainApp::set_progress_active_stage(CategorizationProgressDialog::StageId stage_id)
{
    run_on_ui_blocking([this, stage_id]() {
        if (progress_dialog) {
            progress_dialog->set_active_stage(stage_id);
        }
    });
}

void MainApp::mark_progress_stage_item_in_progress(CategorizationProgressDialog::StageId stage_id,
                                                   const FileEntry& entry)
{
    run_on_ui_blocking([this, stage_id, entry]() {
        if (progress_dialog) {
            progress_dialog->mark_stage_item_in_progress(stage_id, entry);
        }
    });
}

void MainApp::mark_progress_stage_item_completed(CategorizationProgressDialog::StageId stage_id,
                                                 const FileEntry& entry)
{
    run_on_ui_blocking([this, stage_id, entry]() {
        if (progress_dialog) {
            progress_dialog->mark_stage_item_completed(stage_id, entry);
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
    AnalysisCoordinator(*this).execute();
}


void MainApp::run_consistency_pass()
{
    if (stop_analysis.load() || already_categorized_files.empty()) {
        return;
    }

    text_cpu_fallback_choice_.reset();

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
        settings.get_category_language(),
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

bool MainApp::prompt_text_cpu_fallback(const std::string& reason)
{
    if (text_cpu_fallback_choice_.has_value()) {
        return text_cpu_fallback_choice_.value();
    }

    auto show_dialog = [this]() -> bool {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Switch local AI to CPU?"));
        box.setText(tr("The local model encountered a GPU error or ran out of memory."));
        box.setInformativeText(tr("Retry on CPU instead? Cancel will stop this analysis."));
        box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Ok);
        return box.exec() == QMessageBox::Ok;
    };

    bool decision = false;
    if (QThread::currentThread() == thread()) {
        decision = show_dialog();
    } else {
        QMetaObject::invokeMethod(
            this,
            [&decision, show_dialog]() mutable { decision = show_dialog(); },
            Qt::BlockingQueuedConnection);
    }

    text_cpu_fallback_choice_ = decision;

    if (!decision) {
        stop_analysis = true;
        append_progress(to_utf8(tr("[WARN] GPU fallback to CPU declined. Cancelling analysis.")));
        run_on_ui([this]() { request_stop_analysis(); });
        if (core_logger && !reason.empty()) {
            core_logger->warn("GPU fallback declined: {}", reason);
        }
        return false;
    }

    if (core_logger && !reason.empty()) {
        core_logger->warn("GPU fallback accepted: {}", reason);
    }
    return true;
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
            settings.set_llm_downloads_expanded(dialog->get_llm_downloads_expanded());
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
            refresh_backend_status_label();
        }
    } catch (const std::exception& ex) {
        show_error_dialog(fmt::format("LLM selection error: {}", ex.what()));
    }
}

void MainApp::show_suitability_benchmark_dialog(bool /*auto_start*/)
{
    if (benchmark_dialog) {
        benchmark_dialog->raise();
        benchmark_dialog->activateWindow();
        return;
    }

    benchmark_dialog = std::make_unique<SuitabilityBenchmarkDialog>(settings, this);
    QObject::connect(benchmark_dialog.get(), &QDialog::finished, this, [this]() {
        benchmark_dialog.reset();
    });
    benchmark_dialog->show();
}

void MainApp::maybe_show_suitability_benchmark()
{
    if (settings.get_suitability_benchmark_completed()) {
        return;
    }
    if (settings.get_suitability_benchmark_suppressed()) {
        return;
    }
    if (!VisualLlmRuntime::default_text_llm_files_available() && !visual_llm_files_available()) {
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        show_suitability_benchmark_dialog(false);
    });
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
        schedule_backend_status_label_refresh();
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
        schedule_backend_status_label_refresh();
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
        schedule_backend_status_label_refresh();
        return client;
    }

    if (choice == LLMChoice::Custom) {
        const auto id = settings.get_active_custom_llm_id();
        const CustomLLM custom = settings.find_custom_llm(id);
        if (custom.id.empty() || custom.path.empty()) {
            throw std::runtime_error("Selected custom LLM is missing or invalid. Please re-select it.");
        }
        auto client = std::make_unique<LocalLLMClient>(
            custom.path,
            [this](const std::string& reason) { return prompt_text_cpu_fallback(reason); });
        client->set_status_callback([this](LocalLLMClient::Status status) {
            if (status == LocalLLMClient::Status::GpuFallbackToCpu) {
                schedule_backend_status_label_refresh();
                report_progress(to_utf8(tr("[WARN] GPU acceleration failed to initialize. Continuing on CPU (slower).")));
            }
        });
        client->set_prompt_logging_enabled(should_log_prompts());
        schedule_backend_status_label_refresh();
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
        Utils::make_default_path_to_file_from_download_url(env_url),
        [this](const std::string& reason) { return prompt_text_cpu_fallback(reason); });
    client->set_status_callback([this](LocalLLMClient::Status status) {
        if (status == LocalLLMClient::Status::GpuFallbackToCpu) {
            schedule_backend_status_label_refresh();
            report_progress(to_utf8(tr("[WARN] GPU acceleration failed to initialize. Continuing on CPU (slower).")));
        }
    });
    client->set_prompt_logging_enabled(should_log_prompts());
    schedule_backend_status_label_refresh();
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
        categorization_dialog = std::make_unique<CategorizationDialog>(&db_manager,
                                                                       *active_storage_provider_,
                                                                       show_subcategory,
                                                                       undo_dir,
                                                                       settings.get_category_language(),
                                                                       this);
        categorization_dialog->show_results(results,
                                            get_folder_path(),
                                            settings.get_include_subdirectories(),
                                            settings.get_offer_rename_images(),
                                            settings.get_offer_rename_documents());

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
    return normalize_directory_path(
        std::string(bytes.constData(), static_cast<std::size_t>(bytes.size())));
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

void MainApp::run_on_ui_blocking(std::function<void()> func)
{
    if (QThread::currentThread() == thread()) {
        if (func) {
            func();
        }
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [fn = std::move(func)]() mutable {
            if (fn) {
                fn();
            }
        },
        Qt::BlockingQueuedConnection);
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
