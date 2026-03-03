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
class QToolButton;

class Settings;

#include "Language.hpp"
#include "CategoryLanguage.hpp"

/**
 * @brief Applies translated text to the main window UI and related controls.
 */
class UiTranslator
{
public:
    /**
     * @brief References to the primary controls whose labels are translated together.
     */
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
        QPointer<QCheckBox>& include_subdirectories_checkbox;
        QPointer<QCheckBox>& analyze_images_checkbox;
        QPointer<QCheckBox>& process_images_only_checkbox;
        QPointer<QCheckBox>& add_image_date_to_category_checkbox;
        QPointer<QCheckBox>& add_image_date_place_to_filename_checkbox;
        QPointer<QCheckBox>& add_audio_video_metadata_to_filename_checkbox;
        QPointer<QCheckBox>& offer_rename_images_checkbox;
        QPointer<QCheckBox>& rename_images_only_checkbox;
        QPointer<QToolButton>& image_options_toggle_button;
        QPointer<QCheckBox>& analyze_documents_checkbox;
        QPointer<QCheckBox>& process_documents_only_checkbox;
        QPointer<QCheckBox>& offer_rename_documents_checkbox;
        QPointer<QCheckBox>& rename_documents_only_checkbox;
        QPointer<QCheckBox>& add_document_date_to_category_checkbox;
        QPointer<QToolButton>& document_options_toggle_button;
    };

    /**
     * @brief References to top-level menus whose titles are translated.
     */
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

    /**
     * @brief References to menu and command actions whose text is translated.
     */
    struct ActionControls {
        QAction*& file_quit_action;
        QAction*& run_benchmark_action;
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
        QAction*& dutch_action;
        QAction*& french_action;
        QAction*& german_action;
        QAction*& italian_action;
        QAction*& spanish_action;
        QAction*& turkish_action;
        QAction*& korean_action;
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

    /**
     * @brief References to language-selection actions and their action group.
     */
    struct LanguageControls {
        QActionGroup*& language_group;
        QAction*& english_action;
        QAction*& dutch_action;
        QAction*& french_action;
        QAction*& german_action;
        QAction*& italian_action;
        QAction*& spanish_action;
        QAction*& turkish_action;
        QAction*& korean_action;
    };

    /**
     * @brief References to category-language actions and their action group.
     */
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

    /**
     * @brief Runtime state inputs used when translating status-dependent text.
     */
    struct State {
        bool analysis_in_progress{false};
        bool stop_analysis_requested{false};
        bool status_is_ready{true};
    };

    /**
     * @brief Full dependency bundle required to translate the MainApp UI.
     */
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

    /**
     * @brief Constructs a translator wrapper over the provided UI dependencies.
     * @param deps References to the UI controls and translation callback.
     */
    explicit UiTranslator(Dependencies deps);

    /**
     * @brief Retranslates all supported UI text in one pass.
     * @param state Current UI state used for status-dependent labels.
     */
    void retranslate_all(const State& state) const;
    /**
     * @brief Updates the main window title.
     */
    void translate_window_title() const;
    /**
     * @brief Updates the labels of the primary controls on the main window.
     * @param analysis_in_progress True when analysis is currently active.
     */
    void translate_primary_controls(bool analysis_in_progress) const;
    /**
     * @brief Updates translated labels inside the results tree view.
     */
    void translate_tree_view_labels() const;
    /**
     * @brief Updates menu titles and action labels.
     */
    void translate_menus_and_actions() const;
    /**
     * @brief Updates the status bar message for the given runtime state.
     * @param state Current UI state used to choose the status message.
     */
    void translate_status_messages(const State& state) const;
    /**
     * @brief Synchronizes checkmarks for language and category-language actions.
     */
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
