#include <catch2/catch_test_macros.hpp>

#include "Language.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "UiTranslator.hpp"

#include <QAction>
#include <QActionGroup>
#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QComboBox>
#include <QRadioButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QString>

#ifndef _WIN32
namespace {

struct UiTranslatorTestHarness {
    EnvVarGuard platform_guard{"QT_QPA_PLATFORM", "offscreen"};
    QtAppContext qt_context{};
    Settings settings{};
    QMainWindow window{};

    QPointer<QLabel> path_label{new QLabel(&window)};
    QPointer<QPushButton> browse_button{new QPushButton(&window)};
    QPointer<QPushButton> analyze_button{new QPushButton(&window)};
    QPointer<QCheckBox> subcategories_checkbox{new QCheckBox(&window)};
    QPointer<QLabel> style_heading{new QLabel(&window)};
    QPointer<QRadioButton> style_refined{new QRadioButton(&window)};
    QPointer<QRadioButton> style_consistent{new QRadioButton(&window)};
    QPointer<QCheckBox> use_whitelist{new QCheckBox(&window)};
    QPointer<QComboBox> whitelist_selector{new QComboBox(&window)};
    QPointer<QCheckBox> files_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> directories_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> analyze_images_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> process_images_only_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> offer_rename_images_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> rename_images_only_checkbox{new QCheckBox(&window)};

    QPointer<QStandardItemModel> tree_model{new QStandardItemModel(0, 5, &window)};
    QMenu* file_menu = new QMenu(&window);
    QMenu* edit_menu = new QMenu(&window);
    QMenu* view_menu = new QMenu(&window);
    QMenu* settings_menu = new QMenu(&window);
    QMenu* development_menu = new QMenu(&window);
    QMenu* development_settings_menu = new QMenu(&window);
    QMenu* language_menu = new QMenu(&window);
    QMenu* category_language_menu = new QMenu(&window);
    QMenu* help_menu = new QMenu(&window);

    QAction* file_quit_action = new QAction(&window);
    QAction* copy_action = new QAction(&window);
    QAction* cut_action = new QAction(&window);
    QAction* undo_last_run_action = new QAction(&window);
    QAction* paste_action = new QAction(&window);
    QAction* delete_action = new QAction(&window);
    QAction* toggle_explorer_action = new QAction(&window);
    QAction* toggle_llm_action = new QAction(&window);
    QAction* manage_whitelists_action = new QAction(&window);
    QAction* development_prompt_logging_action = new QAction(&window);
    QAction* consistency_pass_action = new QAction(&window);
    QAction* english_action = new QAction(&window);
    QAction* french_action = new QAction(&window);
    QAction* german_action = new QAction(&window);
    QAction* italian_action = new QAction(&window);
    QAction* spanish_action = new QAction(&window);
    QAction* turkish_action = new QAction(&window);
    QAction* category_language_english = new QAction(&window);
    QAction* category_language_french = new QAction(&window);
    QAction* category_language_german = new QAction(&window);
    QAction* category_language_italian = new QAction(&window);
    QAction* category_language_dutch = new QAction(&window);
    QAction* category_language_polish = new QAction(&window);
    QAction* category_language_portuguese = new QAction(&window);
    QAction* category_language_spanish = new QAction(&window);
    QAction* category_language_turkish = new QAction(&window);
    QAction* about_action = new QAction(&window);
    QAction* about_qt_action = new QAction(&window);
    QAction* about_agpl_action = new QAction(&window);
    QAction* support_project_action = new QAction(&window);

    QActionGroup* language_group = new QActionGroup(&window);
    QActionGroup* category_language_group = new QActionGroup(&window);
    QPointer<QDockWidget> file_explorer_dock{new QDockWidget(&window)};

    UiTranslator translator;
    UiTranslator::State state{.analysis_in_progress = false,
                              .stop_analysis_requested = false,
                              .status_is_ready = true};

    UiTranslatorTestHarness()
        : translator(build_deps())
    {
        settings.set_language(Language::French);
        setup_tree_model();
        setup_language_actions();
    }

    void setup_tree_model()
    {
        tree_model->setRowCount(1);
        auto* type_item = new QStandardItem();
        type_item->setData(QStringLiteral("D"), Qt::UserRole);
        tree_model->setItem(0, 1, type_item);
        auto* status_item = new QStandardItem();
        status_item->setData(QStringLiteral("ready"), Qt::UserRole);
        tree_model->setItem(0, 4, status_item);
    }

    void setup_language_actions()
    {
        language_group->setExclusive(true);
        english_action->setCheckable(true);
        english_action->setData(static_cast<int>(Language::English));
        french_action->setCheckable(true);
        french_action->setData(static_cast<int>(Language::French));
        german_action->setCheckable(true);
        italian_action->setCheckable(true);
        spanish_action->setCheckable(true);
        turkish_action->setCheckable(true);
        language_group->addAction(english_action);
        language_group->addAction(french_action);
        language_group->addAction(german_action);
        language_group->addAction(italian_action);
        language_group->addAction(spanish_action);
        language_group->addAction(turkish_action);
    }

    UiTranslator::Dependencies build_deps()
    {
        return UiTranslator::Dependencies{
            .window = window,
            .primary = UiTranslator::PrimaryControls{
                path_label,
                browse_button,
                analyze_button,
                subcategories_checkbox,
                style_heading,
                style_refined,
                style_consistent,
                use_whitelist,
                whitelist_selector,
                files_checkbox,
                directories_checkbox,
                analyze_images_checkbox,
                process_images_only_checkbox,
                offer_rename_images_checkbox,
                rename_images_only_checkbox},
            .tree_model = tree_model,
            .menus = UiTranslator::MenuControls{
                file_menu,
                edit_menu,
                view_menu,
                settings_menu,
                development_menu,
                development_settings_menu,
                language_menu,
                category_language_menu,
                help_menu},
            .actions = UiTranslator::ActionControls{
                file_quit_action,
                copy_action,
                cut_action,
                undo_last_run_action,
                paste_action,
                delete_action,
                toggle_explorer_action,
                toggle_llm_action,
                manage_whitelists_action,
                development_prompt_logging_action,
                consistency_pass_action,
                english_action,
                french_action,
                german_action,
                italian_action,
                spanish_action,
                turkish_action,
                category_language_english,
                category_language_french,
                category_language_german,
                category_language_italian,
                category_language_dutch,
                category_language_polish,
                category_language_portuguese,
                category_language_spanish,
                category_language_turkish,
                about_action,
                about_qt_action,
                about_agpl_action,
                support_project_action},
            .language = UiTranslator::LanguageControls{
                language_group,
                english_action,
                french_action,
                german_action,
                italian_action,
                spanish_action,
                turkish_action},
            .category_language = UiTranslator::CategoryLanguageControls{
                category_language_group,
                category_language_dutch,
                category_language_english,
                category_language_french,
                category_language_german,
                category_language_italian,
                category_language_polish,
                category_language_portuguese,
                category_language_spanish,
                category_language_turkish},
            .file_explorer_dock = file_explorer_dock,
            .settings = settings,
            .translator = [](const char* source) {
                return QString::fromUtf8(source);
            }
        };
    }
};

void verify_primary_controls(const UiTranslatorTestHarness& h)
{
    REQUIRE(h.path_label->text() == QStringLiteral("Folder:"));
    REQUIRE(h.browse_button->text() == QStringLiteral("Browse…"));
    REQUIRE(h.analyze_button->text() == QStringLiteral("Analyze folder"));
    REQUIRE(h.subcategories_checkbox->text() == QStringLiteral("Use subcategories"));
    REQUIRE(h.style_heading->text() == QStringLiteral("Categorization type"));
    REQUIRE(h.style_refined->text() == QStringLiteral("More refined"));
    REQUIRE(h.style_consistent->text() == QStringLiteral("More consistent"));
    REQUIRE(h.use_whitelist->text() == QStringLiteral("Use a whitelist"));
    REQUIRE(h.files_checkbox->text() == QStringLiteral("Categorize files"));
    REQUIRE(h.directories_checkbox->text() == QStringLiteral("Categorize directories"));
    REQUIRE(h.analyze_images_checkbox->text() == QStringLiteral("Analyze picture files by content (can be slow)"));
    REQUIRE(h.process_images_only_checkbox->text() ==
            QStringLiteral("Process picture files only (ignore any other files)"));
    REQUIRE(h.offer_rename_images_checkbox->text() == QStringLiteral("Offer to rename picture files"));
    REQUIRE(h.rename_images_only_checkbox->text() == QStringLiteral("Do not categorize picture files (only rename)"));
}

void verify_menus_and_actions(const UiTranslatorTestHarness& h)
{
    REQUIRE(h.file_menu->title() == QStringLiteral("&File"));
    REQUIRE(h.settings_menu->title() == QStringLiteral("&Settings"));
    REQUIRE(h.toggle_llm_action->text() == QStringLiteral("Select &LLM…"));
    REQUIRE(h.manage_whitelists_action->text() == QStringLiteral("Manage category whitelists…"));
    REQUIRE(h.development_prompt_logging_action->text() ==
            QStringLiteral("Log prompts and responses to stdout"));

    const QString help_title = h.help_menu->title();
    REQUIRE(help_title.endsWith(QStringLiteral("&Help")));
    REQUIRE(help_title.startsWith(QString(QChar(0x200B))));
}

void verify_tree_and_status(const UiTranslatorTestHarness& h)
{
    REQUIRE(h.file_explorer_dock->windowTitle() == QStringLiteral("File Explorer"));
    REQUIRE(h.tree_model->horizontalHeaderItem(0)->text() == QStringLiteral("File"));
    REQUIRE(h.tree_model->item(0, 1)->text() == QStringLiteral("Directory"));
    REQUIRE(h.tree_model->item(0, 4)->text() == QStringLiteral("Ready"));
    REQUIRE_FALSE(h.english_action->isChecked());
    REQUIRE(h.french_action->isChecked());
    REQUIRE(h.window.statusBar()->currentMessage() == QStringLiteral("Ready"));
}

} // namespace

TEST_CASE("UiTranslator updates menus, actions, and controls")
{
    UiTranslatorTestHarness h;
    h.translator.retranslate_all(h.state);
    verify_primary_controls(h);
    verify_menus_and_actions(h);
    verify_tree_and_status(h);
}
#endif
