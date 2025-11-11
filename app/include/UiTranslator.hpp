#ifndef UI_TRANSLATOR_HPP
#define UI_TRANSLATOR_HPP

#include <QPointer>
#include <QString>

#include <functional>

class QAction;
class QActionGroup;
class QCheckBox;
class QDockWidget;
class QLabel;
class QMainWindow;
class QMenu;
class QPushButton;
class QStandardItemModel;

class Settings;

class UiTranslator
{
public:
    struct PrimaryControls {
        QPointer<QLabel>& path_label;
        QPointer<QPushButton>& browse_button;
        QPointer<QPushButton>& analyze_button;
        QPointer<QCheckBox>& use_subcategories_checkbox;
        QPointer<QCheckBox>& categorize_files_checkbox;
        QPointer<QCheckBox>& categorize_directories_checkbox;
    };

    struct MenuControls {
        QMenu*& file_menu;
        QMenu*& edit_menu;
        QMenu*& view_menu;
        QMenu*& settings_menu;
        QMenu*& development_menu;
        QMenu*& development_settings_menu;
        QMenu*& language_menu;
        QMenu*& help_menu;
    };

    struct ActionControls {
        QAction*& file_quit_action;
        QAction*& copy_action;
        QAction*& cut_action;
        QAction*& paste_action;
        QAction*& delete_action;
        QAction*& toggle_explorer_action;
        QAction*& toggle_llm_action;
        QAction*& development_prompt_logging_action;
        QAction*& consistency_pass_action;
        QAction*& english_action;
        QAction*& french_action;
        QAction*& about_action;
        QAction*& about_qt_action;
        QAction*& about_agpl_action;
        QAction*& support_project_action;
    };

    struct LanguageControls {
        QActionGroup*& language_group;
        QAction*& english_action;
        QAction*& french_action;
    };

    struct State {
        bool analysis_in_progress{false};
        bool stop_analysis_requested{false};
        bool status_is_ready{true};
    };

    struct Dependencies {
        QMainWindow& window;
        PrimaryControls primary;
        QPointer<QStandardItemModel>& tree_model;
        MenuControls menus;
        ActionControls actions;
        LanguageControls language;
        QPointer<QDockWidget>& file_explorer_dock;
        Settings& settings;
        std::function<QString(const char*)> translator;
    };

    explicit UiTranslator(Dependencies deps);

    void retranslate_all(const State& state) const;
    void translate_window_title() const;
    void translate_primary_controls(bool analysis_in_progress) const;
    void translate_tree_view_labels() const;
    void translate_menus_and_actions() const;
    void translate_status_messages(const State& state) const;
    void update_language_checks() const;

private:
    QString tr(const char* source) const;

    Dependencies deps_;
};

#endif // UI_TRANSLATOR_HPP
