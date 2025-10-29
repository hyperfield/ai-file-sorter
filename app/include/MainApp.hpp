#ifndef MAINAPP_HPP
#define MAINAPP_HPP

#include "CategorizationDialog.hpp"
#include "CategorizationProgressDialog.hpp"
#include "DatabaseManager.hpp"
#include "FileScanner.hpp"
#include "ILLMClient.hpp"
#include "Settings.hpp"

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

struct CategorizedFile;
struct FileEntry;

class MainApp : public QMainWindow
{
public:
    explicit MainApp(Settings& settings, QWidget* parent = nullptr);
    ~MainApp() override;

    void run();
    void shutdown();

    void show_results_dialog(const std::vector<CategorizedFile>& categorized_files);
    void show_error_dialog(const std::string& message);
    void report_progress(const std::string& message);
    void request_stop_analysis();

    std::vector<FileEntry> get_actual_files(const std::string& directory_path);
    std::vector<CategorizedFile> compute_files_to_sort();
    std::string get_folder_path() const;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setup_ui();
    void setup_menus();
    void setup_file_explorer();
    void connect_signals();
    void connect_edit_actions();
    void start_updater();
    void set_app_icon();

    void load_settings();
    void save_settings();
    void sync_settings_to_ui();
    void sync_ui_to_settings();
    void retranslate_ui();
    void update_language_checks();
    void on_language_selected(Language language);

    void on_analyze_clicked();
    void on_directory_selected(const QString& path);
    void ensure_one_checkbox_active(QCheckBox* changed_checkbox);
    void update_file_scan_option(FileScanOptions option, bool enabled);
    void update_analyze_button_state(bool analyzing);
    void update_results_view_mode();
    void update_folder_contents(const QString& directory);

    void handle_analysis_finished();
    void handle_analysis_failure(const std::string& message);
    void handle_no_files_to_sort();
    void populate_tree_view(const std::vector<CategorizedFile>& files);

    void perform_analysis();
    void stop_running_analysis();
    void show_llm_selection_dialog();
    void on_about_activate();
    void run_consistency_pass();
    std::string build_consistency_prompt(const std::vector<const CategorizedFile*>& chunk,
                                         const std::vector<std::pair<std::string, std::string>>& taxonomy) const;
    void apply_consistency_response(const std::string& response,
                                    std::unordered_map<std::string, CategorizedFile*>& items_by_key,
                                    std::unordered_map<std::string, CategorizedFile*>& new_items_by_key);
    static std::string make_item_key(const CategorizedFile& item);

    std::unordered_set<std::string> extract_file_names(
        const std::vector<CategorizedFile>& categorized_files);
    std::vector<FileEntry> find_files_to_categorize(
        const std::string& directory_path,
        const std::unordered_set<std::string>& cached_files);
    std::vector<CategorizedFile> categorize_files(const std::vector<FileEntry>& files);
    std::optional<CategorizedFile> categorize_single_file(
        ILLMClient& llm, const FileEntry& entry);
    DatabaseManager::ResolvedCategory categorize_file(
        ILLMClient& llm, const std::string& item_name,
        const std::string& item_path,
        const FileType file_type,
        const std::function<void(const std::string&)>& report_progress);
    std::string categorize_with_timeout(
        ILLMClient& llm, const std::string& item_name,
        const std::string& item_path,
        const FileType file_type,
        int timeout_seconds);
    std::unique_ptr<ILLMClient> make_llm_client();

    void run_on_ui(std::function<void()> func);
    void changeEvent(QEvent* event) override;

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
    QMenu* language_menu{nullptr};
    QMenu* help_menu{nullptr};
    QAction* file_quit_action{nullptr};
    QAction* copy_action{nullptr};
    QAction* cut_action{nullptr};
    QAction* paste_action{nullptr};
    QAction* delete_action{nullptr};
    QAction* toggle_explorer_action{nullptr};
    QAction* toggle_llm_action{nullptr};
    QAction* consistency_pass_action{nullptr};
    QActionGroup* language_group{nullptr};
    QAction* english_action{nullptr};
    QAction* french_action{nullptr};
    QAction* about_action{nullptr};
    QAction* about_qt_action{nullptr};
    QAction* about_agpl_action{nullptr};

    std::unique_ptr<CategorizationDialog> categorization_dialog;
    std::unique_ptr<CategorizationProgressDialog> progress_dialog;

    std::shared_ptr<spdlog::logger> core_logger;
    std::shared_ptr<spdlog::logger> ui_logger;

    FileScanOptions file_scan_options{FileScanOptions::None};
    std::thread analyze_thread;
    std::atomic<bool> stop_analysis{false};
    bool analysis_in_progress_{false};
    bool status_is_ready_{true};
};

#endif // MAINAPP_HPP
