#ifndef MAINAPP_HPP
#define MAINAPP_HPP

#include "CategorizationDialog.hpp"
#include "CategorizationProgressDialog.hpp"
#include "DatabaseManager.hpp"
#include "CategorizationService.hpp"
#include "ConsistencyPassService.hpp"
#include "ResultsCoordinator.hpp"
#include "FileScanner.hpp"
#include "ILLMClient.hpp"
#include "Settings.hpp"
#include "WhitelistStore.hpp"
#include "UiTranslator.hpp"
#include "UndoManager.hpp"

#include <QMainWindow>
#include <QPointer>
#include <QStandardItemModel>
#include <QMenu>
#include <QAction>
#include <QActionGroup>

#include "Language.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class QAction;
class QCheckBox;
class QRadioButton;
class QComboBox;
class QLabel;
class QDockWidget;
class QFileSystemModel;
class QLineEdit;
class QString;
class QPushButton;
class QTreeView;
class QStackedWidget;
class QWidget;
class QLabel;
class QEvent;
class MainAppUiBuilder;
class WhitelistManagerDialog;

struct CategorizedFile;
struct FileEntry;

#ifdef AI_FILE_SORTER_TEST_BUILD
class MainAppTestAccess;
#endif

class MainApp : public QMainWindow
{
public:
    enum class SupportPromptResult { Support, NotSure, CannotDonate };
    explicit MainApp(Settings& settings, bool development_mode, QWidget* parent = nullptr);
    ~MainApp() override;

    void run();
    void shutdown();

    void show_results_dialog(const std::vector<CategorizedFile>& categorized_files);
    void show_error_dialog(const std::string& message);
    void report_progress(const std::string& message);
    void request_stop_analysis();

    std::string get_folder_path() const;
    bool is_development_mode() const { return development_mode_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setup_file_explorer();
    void create_file_explorer_dock();
    void setup_file_system_model();
    void setup_file_explorer_view();
    void connect_file_explorer_signals();
    void apply_file_explorer_preferences();
    void restore_tree_settings();
    void restore_sort_folder_state();
    void restore_file_scan_options();
    void restore_file_explorer_visibility();
    void restore_development_preferences();
    void connect_signals();
    void connect_folder_contents_signals();
    void connect_checkbox_signals();
    void connect_whitelist_signals();
    void connect_edit_actions();
    void start_updater();
    void set_app_icon();

    void load_settings();
    void save_settings();
    void sync_settings_to_ui();
    void sync_ui_to_settings();
    void retranslate_ui();
    void on_language_selected(Language language);
    void on_category_language_selected(CategoryLanguage language);
    void initialize_whitelists();

    void on_analyze_clicked();
    void on_directory_selected(const QString& path, bool user_initiated = false);
    void ensure_one_checkbox_active(QCheckBox* changed_checkbox);
    void update_file_scan_option(FileScanOptions option, bool enabled);
    void update_analyze_button_state(bool analyzing);
    void update_results_view_mode();
    void update_folder_contents(const QString& directory);
    void focus_file_explorer_on_path(const QString& path);

    void handle_analysis_finished();
    void handle_analysis_failure(const std::string& message);
    void handle_no_files_to_sort();
    void populate_tree_view(const std::vector<CategorizedFile>& files);

    void perform_analysis();
    void stop_running_analysis();
    void show_llm_selection_dialog();
    void on_about_activate();
    void append_progress(const std::string& message);
    bool should_abort_analysis() const;
    void prune_empty_cached_entries_for(const std::string& directory_path);
    void log_cached_highlights();
    void log_pending_queue();
    void run_consistency_pass();
    void handle_development_prompt_logging(bool checked);
    void record_categorized_metrics(int count);
    SupportPromptResult show_support_prompt_dialog(int categorized_files);
    void undo_last_run();
    bool perform_undo_from_plan(const QString& plan_path);

    std::unique_ptr<ILLMClient> make_llm_client();
    void notify_recategorization_reset(const std::vector<CategorizedFile>& entries,
                                       const std::string& reason);
    void notify_recategorization_reset(const CategorizedFile& entry,
                                       const std::string& reason);
    void set_categorization_style(bool use_consistency);
    bool ensure_folder_categorization_style(const std::string& folder_path);
    void show_whitelist_manager();
    void apply_whitelist_to_selector();

    void run_on_ui(std::function<void()> func);
    void changeEvent(QEvent* event) override;

    friend class MainAppUiBuilder;
#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class MainAppTestAccess;
#endif

    Settings& settings;
    DatabaseManager db_manager;
    FileScanner dirscanner;
    bool using_local_llm{false};

    std::vector<CategorizedFile> already_categorized_files;
    std::vector<CategorizedFile> new_files_with_categories;
    std::vector<FileEntry> files_to_categorize;
    std::vector<CategorizedFile> new_files_to_sort;

    QPointer<QLineEdit> path_entry;
    QPointer<QPushButton> analyze_button;
    QPointer<QPushButton> browse_button;
    QPointer<QLabel> path_label;
    QPointer<QCheckBox> use_subcategories_checkbox;
    QPointer<QLabel> categorization_style_heading;
    QPointer<QRadioButton> categorization_style_refined_radio;
    QPointer<QRadioButton> categorization_style_consistent_radio;
    QPointer<QCheckBox> use_whitelist_checkbox;
    QPointer<QComboBox> whitelist_selector;
    QPointer<QLineEdit> context_input;  // User context input field
    QPointer<QCheckBox> categorize_files_checkbox;
    QPointer<QCheckBox> categorize_directories_checkbox;
    QPointer<QTreeView> tree_view;
    QPointer<QStandardItemModel> tree_model;
    QPointer<QStackedWidget> results_stack;
    QPointer<QTreeView> folder_contents_view;
    QPointer<QFileSystemModel> folder_contents_model;
    int tree_view_page_index_{-1};
    int folder_view_page_index_{-1};

    QPointer<QDockWidget> file_explorer_dock;
    QPointer<QTreeView> file_explorer_view;
    QPointer<QFileSystemModel> file_system_model;
    QAction* file_explorer_menu_action{nullptr};
    QMenu* file_menu{nullptr};
    QMenu* edit_menu{nullptr};
    QMenu* view_menu{nullptr};
    QMenu* settings_menu{nullptr};
    QMenu* development_menu{nullptr};
    QMenu* development_settings_menu{nullptr};
    QMenu* language_menu{nullptr};
    QMenu* category_language_menu{nullptr};
    QMenu* help_menu{nullptr};
    QAction* file_quit_action{nullptr};
    QAction* copy_action{nullptr};
    QAction* cut_action{nullptr};
    QAction* paste_action{nullptr};
    QAction* delete_action{nullptr};
    QAction* undo_last_run_action{nullptr};
    QAction* toggle_explorer_action{nullptr};
    QAction* toggle_llm_action{nullptr};
    QAction* manage_whitelists_action{nullptr};
    QAction* development_prompt_logging_action{nullptr};
    QAction* consistency_pass_action{nullptr};
    QActionGroup* language_group{nullptr};
    QAction* english_action{nullptr};
    QAction* french_action{nullptr};
    QAction* german_action{nullptr};
    QAction* italian_action{nullptr};
    QAction* spanish_action{nullptr};
    QAction* turkish_action{nullptr};
    QActionGroup* category_language_group{nullptr};
    QAction* category_language_dutch{nullptr};
    QAction* category_language_english{nullptr};
    QAction* category_language_french{nullptr};
    QAction* category_language_german{nullptr};
    QAction* category_language_italian{nullptr};
    QAction* category_language_polish{nullptr};
    QAction* category_language_portuguese{nullptr};
    QAction* category_language_spanish{nullptr};
    QAction* category_language_turkish{nullptr};
    QAction* about_action{nullptr};
    QAction* about_qt_action{nullptr};
    QAction* about_agpl_action{nullptr};
    QAction* support_project_action{nullptr};

    std::unique_ptr<CategorizationDialog> categorization_dialog;
    std::unique_ptr<CategorizationProgressDialog> progress_dialog;

    std::shared_ptr<spdlog::logger> core_logger;
    std::shared_ptr<spdlog::logger> ui_logger;
    WhitelistStore whitelist_store;
    std::unique_ptr<WhitelistManagerDialog> whitelist_dialog;
    CategorizationService categorization_service;
    ConsistencyPassService consistency_pass_service;
    ResultsCoordinator results_coordinator;
    UndoManager undo_manager_;
    bool development_mode_{false};
    bool development_prompt_logging_enabled_{false};

    FileScanOptions file_scan_options{FileScanOptions::None};
    std::thread analyze_thread;
    std::atomic<bool> stop_analysis{false};
    bool analysis_in_progress_{false};
    bool status_is_ready_{true};
    bool suppress_explorer_sync_{false};
    bool suppress_folder_view_sync_{false};
    bool donation_prompt_active_{false};
    bool should_log_prompts() const;
    void apply_development_logging();

    std::unique_ptr<UiTranslator> ui_translator_;
};

#endif // MAINAPP_HPP
class WhitelistManagerDialog;
