#include <catch2/catch_test_macros.hpp>

#include "Language.hpp"
#include "MenuMnemonicController.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "UiTranslator.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QComboBox>
#include <QRadioButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QString>
#include <QToolButton>

#include <algorithm>
#include <cstddef>

namespace {

struct UiTranslatorTestHarness {
    EnvVarGuard platform_guard{"QT_QPA_PLATFORM", preferred_qt_test_platform()};
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
    QPointer<QCheckBox> include_subdirectories_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> analyze_images_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> process_images_only_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> add_image_date_to_category_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> add_image_date_place_to_filename_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> add_audio_video_metadata_to_filename_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> offer_rename_images_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> rename_images_only_checkbox{new QCheckBox(&window)};
    QPointer<QToolButton> image_options_toggle_button{new QToolButton(&window)};
    QPointer<QCheckBox> analyze_documents_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> process_documents_only_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> offer_rename_documents_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> rename_documents_only_checkbox{new QCheckBox(&window)};
    QPointer<QCheckBox> add_document_date_to_category_checkbox{new QCheckBox(&window)};
    QPointer<QToolButton> document_options_toggle_button{new QToolButton(&window)};

    QPointer<QStandardItemModel> tree_model{new QStandardItemModel(0, 5, &window)};
    QMenu* file_menu = new QMenu(&window);
    QMenu* edit_menu = new QMenu(&window);
    QMenu* view_menu = new QMenu(&window);
    QMenu* settings_menu = new QMenu(&window);
    QMenu* extensions_menu = new QMenu(&window);
    QMenu* windows_explorer_extension_menu = new QMenu(&window);
    QMenu* plugins_menu = new QMenu(&window);
    QMenu* development_menu = new QMenu(&window);
    QMenu* development_settings_menu = new QMenu(&window);
    QMenu* test_menu = new QMenu(&window);
    QMenu* language_menu = new QMenu(&window);
    QMenu* category_language_menu = new QMenu(&window);
    QMenu* help_menu = new QMenu(&window);

    QAction* file_quit_action = new QAction(&window);
    QAction* run_benchmark_action = new QAction(&window);
    QAction* copy_action = new QAction(&window);
    QAction* cut_action = new QAction(&window);
    QAction* undo_last_run_action = new QAction(&window);
    QAction* review_history_action = new QAction(&window);
    QAction* paste_action = new QAction(&window);
    QAction* delete_action = new QAction(&window);
    QAction* toggle_explorer_action = new QAction(&window);
    QAction* toggle_llm_action = new QAction(&window);
    QAction* manage_storage_plugins_action = new QAction(&window);
    QAction* windows_explorer_extension_install_action = new QAction(&window);
    QAction* windows_explorer_extension_settings_action = new QAction(&window);
    QAction* windows_explorer_extension_activity_action = new QAction(&window);
    QAction* manage_whitelists_action = new QAction(&window);
    QAction* reset_learning_action = new QAction(&window);
    QAction* clear_cache_action = new QAction(&window);
    QAction* development_prompt_logging_action = new QAction(&window);
    QAction* run_large_whitelist_llm_test_action = new QAction(&window);
    QAction* consistency_pass_action = new QAction(&window);
    QAction* english_action = new QAction(&window);
    QAction* dutch_action = new QAction(&window);
    QAction* french_action = new QAction(&window);
    QAction* german_action = new QAction(&window);
    QAction* hindi_action = new QAction(&window);
    QAction* italian_action = new QAction(&window);
    QAction* simplified_chinese_action = new QAction(&window);
    QAction* swedish_action = new QAction(&window);
    QAction* icelandic_action = new QAction(&window);
    QAction* norwegian_action = new QAction(&window);
    QAction* finnish_action = new QAction(&window);
    QAction* danish_action = new QAction(&window);
    QAction* spanish_action = new QAction(&window);
    QAction* turkish_action = new QAction(&window);
    QAction* korean_action = new QAction(&window);
    UiTranslator::CategoryLanguageActionArray category_language_actions{};
    QAction* about_action = new QAction(&window);
    QAction* quick_start_action = new QAction(&window);
    QAction* faq_action = new QAction(&window);
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
        setup_category_language_actions();
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
        dutch_action->setCheckable(true);
        dutch_action->setData(static_cast<int>(Language::Dutch));
        french_action->setCheckable(true);
        french_action->setData(static_cast<int>(Language::French));
        german_action->setCheckable(true);
        german_action->setData(static_cast<int>(Language::German));
        hindi_action->setCheckable(true);
        hindi_action->setData(static_cast<int>(Language::Hindi));
        italian_action->setCheckable(true);
        italian_action->setData(static_cast<int>(Language::Italian));
        simplified_chinese_action->setCheckable(true);
        simplified_chinese_action->setData(static_cast<int>(Language::SimplifiedChinese));
        swedish_action->setCheckable(true);
        swedish_action->setData(static_cast<int>(Language::Swedish));
        icelandic_action->setCheckable(true);
        icelandic_action->setData(static_cast<int>(Language::Icelandic));
        norwegian_action->setCheckable(true);
        norwegian_action->setData(static_cast<int>(Language::Norwegian));
        finnish_action->setCheckable(true);
        finnish_action->setData(static_cast<int>(Language::Finnish));
        danish_action->setCheckable(true);
        danish_action->setData(static_cast<int>(Language::Danish));
        spanish_action->setCheckable(true);
        turkish_action->setCheckable(true);
        spanish_action->setData(static_cast<int>(Language::Spanish));
        turkish_action->setData(static_cast<int>(Language::Turkish));
        korean_action->setCheckable(true);
        korean_action->setData(static_cast<int>(Language::Korean));
        language_group->addAction(english_action);
        language_group->addAction(dutch_action);
        language_group->addAction(french_action);
        language_group->addAction(german_action);
        language_group->addAction(hindi_action);
        language_group->addAction(italian_action);
        language_group->addAction(simplified_chinese_action);
        language_group->addAction(swedish_action);
        language_group->addAction(icelandic_action);
        language_group->addAction(norwegian_action);
        language_group->addAction(finnish_action);
        language_group->addAction(danish_action);
        language_group->addAction(spanish_action);
        language_group->addAction(turkish_action);
        language_group->addAction(korean_action);

        language_menu->addAction(dutch_action);
        language_menu->addAction(english_action);
        language_menu->addAction(french_action);
        language_menu->addAction(german_action);
        language_menu->addAction(hindi_action);
        language_menu->addAction(italian_action);
        language_menu->addAction(simplified_chinese_action);
        language_menu->addAction(swedish_action);
        language_menu->addAction(icelandic_action);
        language_menu->addAction(korean_action);
        language_menu->addAction(norwegian_action);
        language_menu->addAction(finnish_action);
        language_menu->addAction(danish_action);
        language_menu->addAction(spanish_action);
        language_menu->addAction(turkish_action);
    }

    void setup_category_language_actions()
    {
        category_language_group->setExclusive(true);
        for (std::size_t idx = 0; idx < category_language_actions.size(); ++idx) {
            auto* action = new QAction(&window);
            action->setCheckable(true);
            action->setData(static_cast<int>(idx));
            category_language_group->addAction(action);
            category_language_actions[idx] = action;
        }
    }

    QAction* category_language_action(CategoryLanguage language) const
    {
        return category_language_actions[categoryLanguageIndex(language)];
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
                include_subdirectories_checkbox,
                analyze_images_checkbox,
                process_images_only_checkbox,
                add_image_date_to_category_checkbox,
                add_image_date_place_to_filename_checkbox,
                add_audio_video_metadata_to_filename_checkbox,
                offer_rename_images_checkbox,
                rename_images_only_checkbox,
                image_options_toggle_button,
                analyze_documents_checkbox,
                process_documents_only_checkbox,
                offer_rename_documents_checkbox,
                rename_documents_only_checkbox,
                add_document_date_to_category_checkbox,
                document_options_toggle_button},
            .tree_model = tree_model,
            .menus = UiTranslator::MenuControls{
                file_menu,
                edit_menu,
                view_menu,
                settings_menu,
                extensions_menu,
                windows_explorer_extension_menu,
                plugins_menu,
                development_menu,
                development_settings_menu,
                test_menu,
                language_menu,
                category_language_menu,
                help_menu},
            .actions = UiTranslator::ActionControls{
                file_quit_action,
                run_benchmark_action,
                copy_action,
                cut_action,
                undo_last_run_action,
                review_history_action,
                paste_action,
                delete_action,
                toggle_explorer_action,
                toggle_llm_action,
                manage_storage_plugins_action,
                windows_explorer_extension_install_action,
                windows_explorer_extension_settings_action,
                windows_explorer_extension_activity_action,
                manage_whitelists_action,
                reset_learning_action,
                clear_cache_action,
                development_prompt_logging_action,
                run_large_whitelist_llm_test_action,
                consistency_pass_action,
                english_action,
                dutch_action,
                french_action,
                german_action,
                hindi_action,
                italian_action,
                simplified_chinese_action,
                swedish_action,
                icelandic_action,
                norwegian_action,
                finnish_action,
                danish_action,
                spanish_action,
                turkish_action,
                korean_action,
                about_action,
                quick_start_action,
                faq_action,
                about_qt_action,
                about_agpl_action,
                support_project_action},
            .language = UiTranslator::LanguageControls{
                language_group,
                english_action,
                dutch_action,
                french_action,
                german_action,
                hindi_action,
                italian_action,
                simplified_chinese_action,
                swedish_action,
                icelandic_action,
                norwegian_action,
                finnish_action,
                danish_action,
                spanish_action,
                turkish_action,
                korean_action},
            .category_language = UiTranslator::CategoryLanguageControls{
                category_language_group,
                &category_language_actions},
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
    REQUIRE(h.directories_checkbox->text() == QStringLiteral("Categorize folders"));
    REQUIRE(h.include_subdirectories_checkbox->text() == QStringLiteral("Scan subfolders"));
    REQUIRE(h.analyze_images_checkbox->text() == QStringLiteral("Analyze picture files by content"));
    REQUIRE(h.process_images_only_checkbox->text() ==
            QStringLiteral("Process picture files only (ignore any other files)"));
    REQUIRE(h.add_image_date_to_category_checkbox->text() ==
            QStringLiteral("Add image creation date (if available) to category name"));
    REQUIRE(h.add_image_date_place_to_filename_checkbox->text() ==
            QStringLiteral("Add photo date and place to filename (if available)"));
    REQUIRE(h.add_audio_video_metadata_to_filename_checkbox->text() ==
            QStringLiteral("Add audio/video metadata to file name (if available)"));
    REQUIRE(h.offer_rename_images_checkbox->text() == QStringLiteral("Offer to rename picture files"));
    REQUIRE(h.rename_images_only_checkbox->text() == QStringLiteral("Do not categorize picture files (only rename)"));
    REQUIRE(h.analyze_documents_checkbox->text() == QStringLiteral("Analyze document files by content"));
    REQUIRE(h.process_documents_only_checkbox->text() ==
            QStringLiteral("Process document files only (ignore any other files)"));
    REQUIRE(h.offer_rename_documents_checkbox->text() == QStringLiteral("Offer to rename document files"));
    REQUIRE(h.rename_documents_only_checkbox->text() == QStringLiteral("Do not categorize document files (only rename)"));
    REQUIRE(h.add_document_date_to_category_checkbox->text() ==
            QStringLiteral("Add document creation date (if available) to category name"));
}

QStringList menu_action_labels(const QMenu* menu)
{
    QStringList labels;
    if (!menu) {
        return labels;
    }

    for (QAction* const action : menu->actions()) {
        if (action) {
            labels.push_back(action->text().remove(QChar('&')).trimmed());
        }
    }
    return labels;
}

void verify_menus_and_actions(const UiTranslatorTestHarness& h)
{
    REQUIRE(h.file_menu->title() == QStringLiteral("File"));
    REQUIRE(h.edit_menu->title() == QStringLiteral("Edit"));
    REQUIRE(h.view_menu->title() == QStringLiteral("View"));
    REQUIRE(h.settings_menu->title() == QStringLiteral("Settings"));
    REQUIRE(h.extensions_menu->title() == QStringLiteral("Extensions"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.file_menu) == QStringLiteral("&File"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.edit_menu) == QStringLiteral("&Edit"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.view_menu) == QStringLiteral("&View"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.settings_menu) == QStringLiteral("&Settings"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.extensions_menu) == QStringLiteral("E&xtensions"));
    REQUIRE(h.windows_explorer_extension_menu->title() == QStringLiteral("Windows Explorer Extension"));
    REQUIRE(h.plugins_menu->title() == QStringLiteral("Plugins"));
    REQUIRE(h.development_menu->title() == QStringLiteral("Development"));
    REQUIRE(h.test_menu->title() == QStringLiteral("Tests"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.plugins_menu) == QStringLiteral("&Plugins"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.development_menu) == QStringLiteral("&Development"));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.test_menu) == QStringLiteral("&Tests"));
    REQUIRE(h.language_menu->title() == QStringLiteral("Interface &language"));
    REQUIRE(h.category_language_menu->title() == QStringLiteral("Category &language"));
    REQUIRE(h.run_benchmark_action->text() == QStringLiteral("System compatibility check…"));
    REQUIRE(h.review_history_action->text() ==
            QStringLiteral("Rename and categorization history…"));
    REQUIRE(h.toggle_llm_action->text() == QStringLiteral("Select &LLM…"));
    REQUIRE(h.manage_storage_plugins_action->text() == QStringLiteral("Manage storage plugins…"));
    REQUIRE(h.manage_whitelists_action->text() == QStringLiteral("Manage category whitelists…"));
    REQUIRE(h.windows_explorer_extension_install_action->text() ==
            QStringLiteral("Install Windows Explorer Extension..."));
    REQUIRE(h.windows_explorer_extension_settings_action->text() == QStringLiteral("Settings..."));
    REQUIRE(h.windows_explorer_extension_activity_action->text() == QStringLiteral("Activity Window"));
    REQUIRE(h.reset_learning_action->text() == QStringLiteral("Reset learned behavior…"));
    REQUIRE(h.clear_cache_action->text() == QStringLiteral("Clear cache…"));
    REQUIRE(h.category_language_action(CategoryLanguage::Hindi)->text() == QStringLiteral("Hindi"));
    REQUIRE(h.category_language_action(CategoryLanguage::Japanese)->text() == QStringLiteral("Japanese"));
    REQUIRE(h.category_language_action(CategoryLanguage::SimplifiedChinese)->text() ==
            QStringLiteral("Simplified Chinese"));
    REQUIRE(h.category_language_action(CategoryLanguage::TraditionalChinese)->text() ==
            QStringLiteral("Traditional Chinese"));
    REQUIRE(h.quick_start_action->text() == QStringLiteral("&Quick Start Guide"));
    REQUIRE(h.faq_action->text() == QStringLiteral("&FAQ"));
    REQUIRE(h.development_prompt_logging_action->text() ==
            QStringLiteral("Log prompts and responses to stdout"));
    REQUIRE(h.run_large_whitelist_llm_test_action->text() ==
            QStringLiteral("Run large whitelist LLM test…"));

    const QStringList interface_language_labels = menu_action_labels(h.language_menu);
    QStringList sorted_labels = interface_language_labels;
    std::sort(sorted_labels.begin(),
              sorted_labels.end(),
              [](const QString& lhs, const QString& rhs) {
                  return QString::localeAwareCompare(lhs, rhs) < 0;
              });
    REQUIRE(interface_language_labels == sorted_labels);

    const QString help_title = h.help_menu->title();
    REQUIRE(help_title.endsWith(QStringLiteral("Help")));
    REQUIRE(help_title.startsWith(QString(QChar(0x200B))));
    REQUIRE(MenuMnemonicController::mnemonic_title(h.help_menu).endsWith(QStringLiteral("&Help")));
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

TEST_CASE("MenuMnemonicController hides top-level menu mnemonics until activated")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;
    QMainWindow window;
    auto* file_menu = window.menuBar()->addMenu(QString());
    auto* edit_menu = window.menuBar()->addMenu(QString());

    MenuMnemonicController controller(window.menuBar(), &window);
    MenuMnemonicController::set_mnemonic_title(file_menu, QStringLiteral("&File"));
    MenuMnemonicController::set_mnemonic_title(edit_menu, QStringLiteral("&Edit"));
    controller.refresh_titles();

    REQUIRE_FALSE(controller.mnemonics_visible());
    REQUIRE(file_menu->title() == QStringLiteral("File"));
    REQUIRE(edit_menu->title() == QStringLiteral("Edit"));
    REQUIRE(MenuMnemonicController::mnemonic_title(file_menu) == QStringLiteral("&File"));

    QKeyEvent alt_press(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
    QCoreApplication::sendEvent(qApp, &alt_press);
    REQUIRE(controller.mnemonics_visible());
    REQUIRE(file_menu->title() == QStringLiteral("&File"));
    REQUIRE(edit_menu->title() == QStringLiteral("&Edit"));

    QKeyEvent alt_release(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    QCoreApplication::sendEvent(qApp, &alt_release);
    QCoreApplication::processEvents();
    REQUIRE_FALSE(controller.mnemonics_visible());
    REQUIRE(file_menu->title() == QStringLiteral("File"));
    REQUIRE(edit_menu->title() == QStringLiteral("Edit"));

    controller.set_mnemonics_visible(true);
    REQUIRE(controller.mnemonics_visible());
    REQUIRE(file_menu->title() == QStringLiteral("&File"));
    REQUIRE(edit_menu->title() == QStringLiteral("&Edit"));

    controller.set_mnemonics_visible(false);
    REQUIRE_FALSE(controller.mnemonics_visible());
    REQUIRE(file_menu->title() == QStringLiteral("File"));
    REQUIRE(edit_menu->title() == QStringLiteral("Edit"));
}
