#include <catch2/catch_test_macros.hpp>

#include "Language.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "UiTranslator.hpp"

#include <QAction>
#include <QActionGroup>
#include <QChar>
#include <QCheckBox>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QString>

#ifndef _WIN32
TEST_CASE("UiTranslator updates menus, actions, and controls")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    Settings settings;
    settings.set_language(Language::French);

    QMainWindow window;

    QPointer<QLabel> path_label = new QLabel(&window);
    QPointer<QPushButton> browse_button = new QPushButton(&window);
    QPointer<QPushButton> analyze_button = new QPushButton(&window);
    QPointer<QCheckBox> subcategories_checkbox = new QCheckBox(&window);
    QPointer<QCheckBox> files_checkbox = new QCheckBox(&window);
    QPointer<QCheckBox> directories_checkbox = new QCheckBox(&window);

    QPointer<QStandardItemModel> tree_model = new QStandardItemModel(0, 5, &window);
    tree_model->setRowCount(1);
    auto* type_item = new QStandardItem();
    type_item->setData(QStringLiteral("D"), Qt::UserRole);
    tree_model->setItem(0, 1, type_item);
    auto* status_item = new QStandardItem();
    status_item->setData(QStringLiteral("ready"), Qt::UserRole);
    tree_model->setItem(0, 4, status_item);

    QMenu* file_menu = new QMenu(&window);
    QMenu* edit_menu = new QMenu(&window);
    QMenu* view_menu = new QMenu(&window);
    QMenu* settings_menu = new QMenu(&window);
    QMenu* development_menu = new QMenu(&window);
    QMenu* development_settings_menu = new QMenu(&window);
    QMenu* language_menu = new QMenu(&window);
    QMenu* help_menu = new QMenu(&window);

    QAction* file_quit_action = new QAction(&window);
    QAction* copy_action = new QAction(&window);
    QAction* cut_action = new QAction(&window);
    QAction* paste_action = new QAction(&window);
    QAction* delete_action = new QAction(&window);
    QAction* toggle_explorer_action = new QAction(&window);
    QAction* toggle_llm_action = new QAction(&window);
    QAction* development_prompt_logging_action = new QAction(&window);
    QAction* consistency_pass_action = new QAction(&window);
    QAction* english_action = new QAction(&window);
    QAction* french_action = new QAction(&window);
    QAction* about_action = new QAction(&window);
    QAction* about_qt_action = new QAction(&window);
    QAction* about_agpl_action = new QAction(&window);
    QAction* support_project_action = new QAction(&window);

    QActionGroup* language_group = new QActionGroup(&window);
    language_group->setExclusive(true);
    english_action->setCheckable(true);
    english_action->setData(static_cast<int>(Language::English));
    french_action->setCheckable(true);
    french_action->setData(static_cast<int>(Language::French));
    language_group->addAction(english_action);
    language_group->addAction(french_action);

    QPointer<QDockWidget> file_explorer_dock = new QDockWidget(&window);

    UiTranslator::Dependencies deps{
        .window = window,
        .primary = UiTranslator::PrimaryControls{
            path_label,
            browse_button,
            analyze_button,
            subcategories_checkbox,
            files_checkbox,
            directories_checkbox},
        .tree_model = tree_model,
        .menus = UiTranslator::MenuControls{
            file_menu,
            edit_menu,
            view_menu,
            settings_menu,
            development_menu,
            development_settings_menu,
            language_menu,
            help_menu},
        .actions = UiTranslator::ActionControls{
            file_quit_action,
            copy_action,
            cut_action,
            paste_action,
            delete_action,
            toggle_explorer_action,
            toggle_llm_action,
            development_prompt_logging_action,
            consistency_pass_action,
            english_action,
            french_action,
            about_action,
            about_qt_action,
            about_agpl_action,
            support_project_action},
        .language = UiTranslator::LanguageControls{
            language_group,
            english_action,
            french_action},
        .file_explorer_dock = file_explorer_dock,
        .settings = settings,
        .translator = [](const char* source) {
            return QString::fromUtf8(source);
        }
    };

    UiTranslator translator(deps);
    UiTranslator::State state{
        .analysis_in_progress = false,
        .stop_analysis_requested = false,
        .status_is_ready = true};

    translator.retranslate_all(state);

    REQUIRE(path_label->text() == QStringLiteral("Folder:"));
    REQUIRE(browse_button->text() == QStringLiteral("Browse…"));
    REQUIRE(analyze_button->text() == QStringLiteral("Analyze folder"));
    REQUIRE(subcategories_checkbox->text() == QStringLiteral("Use subcategories"));
    REQUIRE(files_checkbox->text() == QStringLiteral("Categorize files"));
    REQUIRE(directories_checkbox->text() == QStringLiteral("Categorize directories"));

    REQUIRE(file_menu->title() == QStringLiteral("&File"));
    REQUIRE(settings_menu->title() == QStringLiteral("&Settings"));
    REQUIRE(toggle_llm_action->text() == QStringLiteral("Select &LLM…"));
    REQUIRE(development_prompt_logging_action->text() ==
            QStringLiteral("Log prompts and responses to stdout"));

    const QString help_title = help_menu->title();
    REQUIRE(help_title.endsWith(QStringLiteral("&Help")));
    REQUIRE(help_title.startsWith(QString(QChar(0x200B))));

    REQUIRE(file_explorer_dock->windowTitle() == QStringLiteral("File Explorer"));
    REQUIRE(tree_model->horizontalHeaderItem(0)->text() == QStringLiteral("File"));
    REQUIRE(type_item->text() == QStringLiteral("Directory"));
    REQUIRE(status_item->text() == QStringLiteral("Ready"));

    REQUIRE_FALSE(english_action->isChecked());
    REQUIRE(french_action->isChecked());
    REQUIRE(window.statusBar()->currentMessage() == QStringLiteral("Ready"));
}
#endif
