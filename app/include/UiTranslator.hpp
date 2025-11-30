#ifndef UI_TRANSLATOR_HPP
#define UI_TRANSLATOR_HPP

#include <QPointer>
#include <QString>

#include <functional>

#include <QCheckBox>
#include <QRadioButton>

class QAction;
class QActionGroup;

class QDockWidget;
class QLabel;
class QMainWindow;
class QMenu;
class QPushButton;
class QComboBox;
class QStandardItemModel;

class Settings;

#include "Language.hpp"
#include "CategoryLanguage.hpp"

class UiTranslator
{
public:
    struct PrimaryControls {
        QPointer<QLabel>& path_label;
        QPointer<QPushButton>& browse_button;
        QPointer<QPushButton>& analyze_button;
        QPointer<QCheckBox>& use_subcategories_checkbox;
        QPointer<QLabel>& categorization_style_heading;
        QPointer<QRadioButton>& categorization_style_refined_radio;
        QPointer<QRadioButton>& categorization_style_consistent_radio;
        QPointer<QCheckBox>& use_whitelist_checkbox;
        QPointer<QComboBox>& whitelist_selector;
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
        QMenu*& category_language_menu;
        QMenu*& help_menu;
    };

    struct ActionControls {
        QAction*& file_quit_action;
        QAction*& copy_action;
        QAction*& cut_action;
        QAction*& undo_last_run_action;
        QAction*& paste_action;
        QAction*& delete_action;
        QAction*& toggle_explorer_action;
        QAction*& toggle_llm_action;
        QAction*& manage_whitelists_action;
        QAction*& development_prompt_logging_action;
        QAction*& consistency_pass_action;
        QAction*& english_action;
        QAction*& french_action;
        QAction*& german_action;
        QAction*& italian_action;
        QAction*& spanish_action;
        QAction*& turkish_action;
        QAction*& category_language_english;
        QAction*& category_language_french;
        QAction*& category_language_german;
        QAction*& category_language_italian;
        QAction*& category_language_dutch;
        QAction*& category_language_polish;
        QAction*& category_language_portuguese;
        QAction*& category_language_spanish;
        QAction*& category_language_turkish;
        QAction*& about_action;
        QAction*& about_qt_action;
        QAction*& about_agpl_action;
        QAction*& support_project_action;
    };

    struct LanguageControls {
        QActionGroup*& language_group;
        QAction*& english_action;
        QAction*& french_action;
        QAction*& german_action;
        QAction*& italian_action;
        QAction*& spanish_action;
        QAction*& turkish_action;
    };

    struct CategoryLanguageControls {
        QActionGroup*& category_language_group;
        QAction*& dutch;
        QAction*& english;
        QAction*& french;
        QAction*& german;
        QAction*& italian;
        QAction*& polish;
        QAction*& portuguese;
        QAction*& spanish;
        QAction*& turkish;
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
        CategoryLanguageControls category_language;
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
    void update_language_group_checks(Language configured) const;
    void update_category_language_checks(CategoryLanguage configured) const;

    Dependencies deps_;
};

#endif // UI_TRANSLATOR_HPP
#include "Language.hpp"
#include "CategoryLanguage.hpp"
