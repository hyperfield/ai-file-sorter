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

    QPointer<QStandardItemModel> tree_model{new QStandardItemModel(0, 5, &window)};
    QMenu file_menu{&window}, edit_menu{&window}, view_menu{&window}, settings_menu{&window},
         development_menu{&window}, development_settings_menu{&window}, language_menu{&window},
         help_menu{&window};

    QAction file_quit_action{&window}, copy_action{&window}, cut_action{&window},
        paste_action{&window}, delete_action{&window}, toggle_explorer_action{&window},
        toggle_llm_action{&window}, manage_whitelists_action{&window},
        development_prompt_logging_action{&window}, consistency_pass_action{&window},
        english_action{&window}, french_action{&window}, about_action{&window},
        about_qt_action{&window}, about_agpl_action{&window}, support_project_action{&window};

    QActionGroup language_group{&window};
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
        language_group.setExclusive(true);
        english_action.setCheckable(true);
        english_action.setData(static_cast<int>(Language::English));
        french_action.setCheckable(true);
        french_action.setData(static_cast<int>(Language::French));
        language_group.addAction(&english_action);
        language_group.addAction(&french_action);
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
                directories_checkbox},
            .tree_model = tree_model,
            .menus = UiTranslator::MenuControls{
                &file_menu,
                &edit_menu,
                &view_menu,
                &settings_menu,
                &development_menu,
                &development_settings_menu,
                &language_menu,
                &help_menu},
            .actions = UiTranslator::ActionControls{
                &file_quit_action,
                &copy_action,
                &cut_action,
                &paste_action,
                &delete_action,
                &toggle_explorer_action,
                &toggle_llm_action,
                &manage_whitelists_action,
                &development_prompt_logging_action,
                &consistency_pass_action,
                &english_action,
                &french_action,
                &about_action,
                &about_qt_action,
                &about_agpl_action,
                &support_project_action},
            .language = UiTranslator::LanguageControls{
                &language_group,
                &english_action,
                &french_action},
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
}

void verify_menus_and_actions(const UiTranslatorTestHarness& h)
{
    REQUIRE(h.file_menu.title() == QStringLiteral("&File"));
    REQUIRE(h.settings_menu.title() == QStringLiteral("&Settings"));
    REQUIRE(h.toggle_llm_action.text() == QStringLiteral("Select &LLM…"));
    REQUIRE(h.manage_whitelists_action.text() == QStringLiteral("Manage category whitelists…"));
    REQUIRE(h.development_prompt_logging_action.text() ==
            QStringLiteral("Log prompts and responses to stdout"));

    const QString help_title = h.help_menu.title();
    REQUIRE(help_title.endsWith(QStringLiteral("&Help")));
    REQUIRE(help_title.startsWith(QString(QChar(0x200B))));
}

void verify_tree_and_status(const UiTranslatorTestHarness& h)
{
    REQUIRE(h.file_explorer_dock->windowTitle() == QStringLiteral("File Explorer"));
    REQUIRE(h.tree_model->horizontalHeaderItem(0)->text() == QStringLiteral("File"));
    REQUIRE(h.tree_model->item(0, 1)->text() == QStringLiteral("Directory"));
    REQUIRE(h.tree_model->item(0, 4)->text() == QStringLiteral("Ready"));
    REQUIRE_FALSE(h.english_action.isChecked());
    REQUIRE(h.french_action.isChecked());
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
