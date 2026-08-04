#include "UiTranslator.hpp"

#include "Language.hpp"
#include "CategoryLanguage.hpp"
#include "MenuMnemonicController.hpp"
#include "Settings.hpp"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringList>

#include <algorithm>

namespace {

constexpr auto kMenuTitleFile = QT_TRANSLATE_NOOP("UiTranslator", "&File");
constexpr auto kMenuTitleEdit = QT_TRANSLATE_NOOP("UiTranslator", "&Edit");
constexpr auto kMenuTitleView = QT_TRANSLATE_NOOP("UiTranslator", "&View");
constexpr auto kMenuTitleSettings = QT_TRANSLATE_NOOP("UiTranslator", "&Settings");
constexpr auto kMenuTitleExtensions = QT_TRANSLATE_NOOP("UiTranslator", "E&xtensions");
constexpr auto kMenuTitleWindowsExplorerExtension =
    QT_TRANSLATE_NOOP("UiTranslator", "Windows Explorer Extension");
constexpr auto kMenuTitlePlugins = QT_TRANSLATE_NOOP("UiTranslator", "&Plugins");
constexpr auto kMenuTitleDevelopment = QT_TRANSLATE_NOOP("UiTranslator", "&Development");
constexpr auto kMenuTitleTests = QT_TRANSLATE_NOOP("UiTranslator", "&Tests");
constexpr auto kMenuTitleInterfaceLanguage =
    QT_TRANSLATE_NOOP("UiTranslator", "Interface &language");
constexpr auto kMenuTitleCategoryLanguage =
    QT_TRANSLATE_NOOP("UiTranslator", "Category &language");
constexpr auto kActionQuit = QT_TRANSLATE_NOOP("UiTranslator", "&Quit");
constexpr auto kActionSystemCompatibilityCheck =
    QT_TRANSLATE_NOOP("UiTranslator", "System compatibility check…");
constexpr auto kActionCopy = QT_TRANSLATE_NOOP("UiTranslator", "&Copy");
constexpr auto kActionCut = QT_TRANSLATE_NOOP("UiTranslator", "Cu&t");
constexpr auto kActionUndoLastRun = QT_TRANSLATE_NOOP("UiTranslator", "Undo last run");
constexpr auto kActionReviewHistory =
    QT_TRANSLATE_NOOP("UiTranslator", "Rename and categorization history…");
constexpr auto kActionPaste = QT_TRANSLATE_NOOP("UiTranslator", "&Paste");
constexpr auto kActionDelete = QT_TRANSLATE_NOOP("UiTranslator", "&Delete");
constexpr auto kActionFileExplorer = QT_TRANSLATE_NOOP("UiTranslator", "File &Explorer");
constexpr auto kActionSelectLlm = QT_TRANSLATE_NOOP("UiTranslator", "Select &LLM…");
constexpr auto kActionManageStoragePlugins =
    QT_TRANSLATE_NOOP("UiTranslator", "Manage storage plugins…");
constexpr auto kActionManageCategoryWhitelists =
    QT_TRANSLATE_NOOP("UiTranslator", "Manage category whitelists…");
constexpr auto kActionInstallWindowsExplorerExtension =
    QT_TRANSLATE_NOOP("UiTranslator", "Install Windows Explorer Extension...");
constexpr auto kActionWindowsExplorerExtensionSettings =
    QT_TRANSLATE_NOOP("UiTranslator", "Settings...");
constexpr auto kActionWindowsExplorerExtensionActivity =
    QT_TRANSLATE_NOOP("UiTranslator", "Activity Window");
constexpr auto kActionResetLearnedBehavior =
    QT_TRANSLATE_NOOP("UiTranslator", "Reset learned behavior…");
constexpr auto kActionClearCache = QT_TRANSLATE_NOOP("UiTranslator", "Clear cache…");
constexpr auto kActionPromptLogging =
    QT_TRANSLATE_NOOP("UiTranslator", "Log prompts and responses to stdout");
constexpr auto kActionRunLargeWhitelistLlmTest =
    QT_TRANSLATE_NOOP("UiTranslator", "Run large whitelist LLM test…");
constexpr auto kActionRunConsistencyPass =
    QT_TRANSLATE_NOOP("UiTranslator", "Run &consistency pass");
constexpr auto kActionEnglish = QT_TRANSLATE_NOOP("UiTranslator", "&English");
constexpr auto kActionDutch = QT_TRANSLATE_NOOP("UiTranslator", "&Dutch");
constexpr auto kActionFrench = QT_TRANSLATE_NOOP("UiTranslator", "&French");
constexpr auto kActionGerman = QT_TRANSLATE_NOOP("UiTranslator", "&German");
constexpr auto kActionHindi = QT_TRANSLATE_NOOP("UiTranslator", "&Hindi");
constexpr auto kActionItalian = QT_TRANSLATE_NOOP("UiTranslator", "&Italian");
constexpr auto kActionSimplifiedChinese =
    QT_TRANSLATE_NOOP("UiTranslator", "&Simplified Chinese");
constexpr auto kActionSwedish = QT_TRANSLATE_NOOP("UiTranslator", "&Swedish");
constexpr auto kActionIcelandic = QT_TRANSLATE_NOOP("UiTranslator", "&Icelandic");
constexpr auto kActionNorwegian = QT_TRANSLATE_NOOP("UiTranslator", "&Norwegian");
constexpr auto kActionFinnish = QT_TRANSLATE_NOOP("UiTranslator", "&Finnish");
constexpr auto kActionDanish = QT_TRANSLATE_NOOP("UiTranslator", "&Danish");
constexpr auto kActionSpanish = QT_TRANSLATE_NOOP("UiTranslator", "&Spanish");
constexpr auto kActionTurkish = QT_TRANSLATE_NOOP("UiTranslator", "&Turkish");
constexpr auto kActionKorean = QT_TRANSLATE_NOOP("UiTranslator", "&Korean");
constexpr auto kActionAboutAiFileSorter =
    QT_TRANSLATE_NOOP("UiTranslator", "&About AI File Sorter");
constexpr auto kActionQuickStartGuide =
    QT_TRANSLATE_NOOP("UiTranslator", "&Quick Start Guide");
constexpr auto kActionFaq = QT_TRANSLATE_NOOP("UiTranslator", "&FAQ");
constexpr auto kActionAboutQt = QT_TRANSLATE_NOOP("UiTranslator", "About &Qt");
constexpr auto kActionAboutAgpl = QT_TRANSLATE_NOOP("UiTranslator", "About &AGPL");
constexpr auto kActionSupportProject =
    QT_TRANSLATE_NOOP("UiTranslator", "&Support Project");
constexpr auto kMenuTitleHelp = QT_TRANSLATE_NOOP("UiTranslator", "&Help");
constexpr auto kDockTitleFileExplorer = QT_TRANSLATE_NOOP("UiTranslator", "File Explorer");

template <typename Widget>
Widget* raw_ptr(const QPointer<Widget>& pointer)
{
    return pointer.data();
}

QString menu_action_sort_key(const QAction* action)
{
    if (!action) {
        return QString();
    }
    return MenuMnemonicController::strip_mnemonic_markers(action->text()).trimmed();
}

void sort_menu_actions_by_label(QMenu* menu)
{
    if (!menu) {
        return;
    }

    QList<QAction*> actions = menu->actions();
    std::sort(actions.begin(),
              actions.end(),
              [](QAction* lhs, QAction* rhs) {
                  return QString::localeAwareCompare(menu_action_sort_key(lhs),
                                                     menu_action_sort_key(rhs)) < 0;
              });

    for (QAction* const action : actions) {
        menu->removeAction(action);
    }
    for (QAction* const action : actions) {
        menu->addAction(action);
    }
}

QAction* shared_interface_language_action(const UiTranslator::Dependencies& deps,
                                          CategoryLanguage language)
{
    switch (language) {
    case CategoryLanguage::Danish: return deps.actions.danish_action;
    case CategoryLanguage::Dutch: return deps.actions.dutch_action;
    case CategoryLanguage::English: return deps.actions.english_action;
    case CategoryLanguage::Finnish: return deps.actions.finnish_action;
    case CategoryLanguage::French: return deps.actions.french_action;
    case CategoryLanguage::German: return deps.actions.german_action;
    case CategoryLanguage::Hindi: return deps.actions.hindi_action;
    case CategoryLanguage::Icelandic: return deps.actions.icelandic_action;
    case CategoryLanguage::Italian: return deps.actions.italian_action;
    case CategoryLanguage::Korean: return deps.actions.korean_action;
    case CategoryLanguage::Norwegian: return deps.actions.norwegian_action;
    case CategoryLanguage::SimplifiedChinese: return deps.actions.simplified_chinese_action;
    case CategoryLanguage::Spanish: return deps.actions.spanish_action;
    case CategoryLanguage::Swedish: return deps.actions.swedish_action;
    case CategoryLanguage::Turkish: return deps.actions.turkish_action;
    default: return nullptr;
    }
}

} // namespace

UiTranslator::UiTranslator(Dependencies deps)
    : deps_(deps)
{
    if (!deps_.translator) {
        deps_.translator = [](const char* source) {
            return QCoreApplication::translate("UiTranslator", source);
        };
    }
}

void UiTranslator::retranslate_all(const State& state) const
{
    translate_window_title();
    translate_primary_controls(state.analysis_in_progress);
    translate_tree_view_labels();
    translate_menus_and_actions();
    translate_status_messages(state);
    update_language_checks();
}

void UiTranslator::translate_window_title() const
{
    deps_.window.setWindowTitle(QStringLiteral("AI File Sorter"));
}

void UiTranslator::translate_primary_controls(bool analysis_in_progress) const
{
    if (auto* label = raw_ptr(deps_.primary.path_label)) {
        label->setText(tr("Folder:"));
    }
    if (auto* button = raw_ptr(deps_.primary.browse_button)) {
        button->setText(tr("Browse…"));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.use_subcategories_checkbox)) {
        checkbox->setText(tr("Use subcategories"));
        checkbox->setToolTip(tr("Create subcategory folders within each category."));
    }
    if (auto* heading = raw_ptr(deps_.primary.categorization_style_heading)) {
        heading->setText(tr("Categorization type"));
        heading->setToolTip(tr("Choose how strict the category labels should be."));
    }
    if (auto* refined_radio = raw_ptr(deps_.primary.categorization_style_refined_radio)) {
        refined_radio->setText(tr("More refined"));
        refined_radio->setToolTip(tr("Favor detailed labels even if similar items vary."));
    }
    if (auto* consistent_radio = raw_ptr(deps_.primary.categorization_style_consistent_radio)) {
        consistent_radio->setText(tr("More consistent"));
        consistent_radio->setToolTip(tr("Favor consistent labels across similar items."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.use_whitelist_checkbox)) {
        checkbox->setText(tr("Use a whitelist"));
        checkbox->setToolTip(tr("Restrict categories and subcategories to the selected whitelist."));
    }
    if (auto* selector = raw_ptr(deps_.primary.whitelist_selector)) {
        selector->setToolTip(tr("Select the whitelist used for this run."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.categorize_files_checkbox)) {
        checkbox->setText(tr("Categorize files"));
        checkbox->setToolTip(tr("Include files in the categorization pass."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.categorize_directories_checkbox)) {
        checkbox->setText(tr("Categorize folders"));
        checkbox->setToolTip(tr("Include directories in the categorization pass."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.include_subdirectories_checkbox)) {
        checkbox->setText(tr("Scan subfolders"));
        checkbox->setToolTip(tr("Scan files inside subfolders and treat them as part of the main folder."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.analyze_images_checkbox)) {
        checkbox->setText(tr("Analyze picture files by content"));
        checkbox->setToolTip(tr("Run the visual LLM on supported picture files."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.process_images_only_checkbox)) {
        checkbox->setText(tr("Process picture files only (ignore any other files)"));
        checkbox->setToolTip(tr("Ignore non-picture files in this run."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.add_image_date_to_category_checkbox)) {
        checkbox->setText(tr("Add image creation date (if available) to category name"));
        checkbox->setToolTip(tr("Append the image creation date from metadata to the category label."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.add_image_date_place_to_filename_checkbox)) {
        checkbox->setText(tr("Add photo date and place to filename (if available)"));
        checkbox->setToolTip(tr("Date comes from photo EXIF metadata. Place names are resolved online from GPS coordinates, so network access is required for place prefixes."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.add_audio_video_metadata_to_filename_checkbox)) {
        checkbox->setText(tr("Add audio/video metadata to file name (if available)"));
        checkbox->setToolTip(tr("Use embedded media tags (for example year, artist, album, title) to build suggested audio/video filenames."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.offer_rename_images_checkbox)) {
        checkbox->setText(tr("Offer to rename picture files"));
        checkbox->setToolTip(tr("Show suggested filenames for picture files."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.rename_images_only_checkbox)) {
        checkbox->setText(tr("Do not categorize picture files (only rename)"));
        checkbox->setToolTip(tr("Skip categorization for picture files and only rename them."));
    }
    if (auto* button = raw_ptr(deps_.primary.image_options_toggle_button)) {
        button->setToolTip(tr("Show or hide picture analysis options"));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.analyze_documents_checkbox)) {
        checkbox->setText(tr("Analyze document files by content"));
        checkbox->setToolTip(tr("Summarize document contents with the selected LLM."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.process_documents_only_checkbox)) {
        checkbox->setText(tr("Process document files only (ignore any other files)"));
        checkbox->setToolTip(tr("Ignore non-document files in this run."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.offer_rename_documents_checkbox)) {
        checkbox->setText(tr("Offer to rename document files"));
        checkbox->setToolTip(tr("Show suggested filenames for document files."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.rename_documents_only_checkbox)) {
        checkbox->setText(tr("Do not categorize document files (only rename)"));
        checkbox->setToolTip(tr("Skip categorization for document files and only rename them."));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.add_document_date_to_category_checkbox)) {
        checkbox->setText(tr("Add document creation date (if available) to category name"));
        checkbox->setToolTip(tr("Append the document creation date from metadata to the category label."));
    }
    if (auto* button = raw_ptr(deps_.primary.document_options_toggle_button)) {
        button->setToolTip(tr("Show or hide document analysis options"));
    }
    if (auto* button = raw_ptr(deps_.primary.analyze_button)) {
        button->setText(analysis_in_progress ? tr("Stop analyzing") : tr("Analyze folder"));
    }
}

void UiTranslator::translate_tree_view_labels() const
{
    QStandardItemModel* model = raw_ptr(deps_.tree_model);
    if (!model) {
        return;
    }

    model->setHorizontalHeaderLabels(QStringList{
        tr("File"),
        tr("Type"),
        tr("Category"),
        tr("Subcategory"),
        tr("Status")
    });

    for (int row = 0; row < model->rowCount(); ++row) {
        if (auto* type_item = model->item(row, 1)) {
            const QString type_code = type_item->data(Qt::UserRole).toString();
            if (type_code == QStringLiteral("D")) {
                type_item->setText(tr("Directory"));
            } else if (type_code == QStringLiteral("F")) {
                type_item->setText(tr("File"));
            }
        }
        if (auto* status_item = model->item(row, 4)) {
            const QString status_code = status_item->data(Qt::UserRole).toString();
            if (status_code == QStringLiteral("ready")) {
                status_item->setText(tr("Ready"));
            }
        }
    }
}

void UiTranslator::translate_menus_and_actions() const
{
    struct MenuEntry {
        QMenu* menu{nullptr};
        const char* text{nullptr};
        bool top_level{false};
    };

    const MenuEntry menu_entries[] = {
        {deps_.menus.file_menu, kMenuTitleFile, true},
        {deps_.menus.edit_menu, kMenuTitleEdit, true},
        {deps_.menus.view_menu, kMenuTitleView, true},
        {deps_.menus.settings_menu, kMenuTitleSettings, true},
        {deps_.menus.extensions_menu, kMenuTitleExtensions, true},
        {deps_.menus.windows_explorer_extension_menu, kMenuTitleWindowsExplorerExtension, false},
        {deps_.menus.plugins_menu, kMenuTitlePlugins, true},
        {deps_.menus.development_menu, kMenuTitleDevelopment, true},
        {deps_.menus.development_settings_menu, kMenuTitleSettings, false},
        {deps_.menus.test_menu, kMenuTitleTests, true},
        {deps_.menus.language_menu, kMenuTitleInterfaceLanguage, false},
        {deps_.menus.category_language_menu, kMenuTitleCategoryLanguage, false}
    };

    for (const MenuEntry& entry : menu_entries) {
        if (entry.menu && entry.text) {
            const QString translated = tr(entry.text);
            if (entry.top_level) {
                MenuMnemonicController::set_mnemonic_title(entry.menu, translated);
            } else {
                entry.menu->setTitle(translated);
            }
        }
    }

    struct ActionEntry {
        QAction* action{nullptr};
        const char* text{nullptr};
    };

    const ActionEntry action_entries[] = {
        {deps_.actions.file_quit_action, kActionQuit},
        {deps_.actions.run_benchmark_action, kActionSystemCompatibilityCheck},
        {deps_.actions.copy_action, kActionCopy},
        {deps_.actions.cut_action, kActionCut},
        {deps_.actions.undo_last_run_action, kActionUndoLastRun},
        {deps_.actions.review_history_action, kActionReviewHistory},
        {deps_.actions.paste_action, kActionPaste},
        {deps_.actions.delete_action, kActionDelete},
        {deps_.actions.toggle_explorer_action, kActionFileExplorer},
        {deps_.actions.toggle_llm_action, kActionSelectLlm},
        {deps_.actions.manage_storage_plugins_action, kActionManageStoragePlugins},
        {deps_.actions.windows_explorer_extension_install_action, kActionInstallWindowsExplorerExtension},
        {deps_.actions.windows_explorer_extension_settings_action, kActionWindowsExplorerExtensionSettings},
        {deps_.actions.windows_explorer_extension_activity_action, kActionWindowsExplorerExtensionActivity},
        {deps_.actions.manage_whitelists_action, kActionManageCategoryWhitelists},
        {deps_.actions.reset_learning_action, kActionResetLearnedBehavior},
        {deps_.actions.clear_cache_action, kActionClearCache},
        {deps_.actions.development_prompt_logging_action, kActionPromptLogging},
        {deps_.actions.run_large_whitelist_llm_test_action, kActionRunLargeWhitelistLlmTest},
        {deps_.actions.consistency_pass_action, kActionRunConsistencyPass},
        {deps_.actions.english_action, kActionEnglish},
        {deps_.actions.dutch_action, kActionDutch},
        {deps_.actions.french_action, kActionFrench},
        {deps_.actions.german_action, kActionGerman},
        {deps_.actions.hindi_action, kActionHindi},
        {deps_.actions.italian_action, kActionItalian},
        {deps_.actions.simplified_chinese_action, kActionSimplifiedChinese},
        {deps_.actions.swedish_action, kActionSwedish},
        {deps_.actions.icelandic_action, kActionIcelandic},
        {deps_.actions.norwegian_action, kActionNorwegian},
        {deps_.actions.finnish_action, kActionFinnish},
        {deps_.actions.danish_action, kActionDanish},
        {deps_.actions.spanish_action, kActionSpanish},
        {deps_.actions.turkish_action, kActionTurkish},
        {deps_.actions.korean_action, kActionKorean},
        {deps_.actions.about_action, kActionAboutAiFileSorter},
        {deps_.actions.quick_start_action, kActionQuickStartGuide},
        {deps_.actions.faq_action, kActionFaq},
        {deps_.actions.about_qt_action, kActionAboutQt},
        {deps_.actions.about_agpl_action, kActionAboutAgpl},
        {deps_.actions.support_project_action, kActionSupportProject}
    };

    for (const ActionEntry& entry : action_entries) {
        if (entry.action && entry.text) {
            entry.action->setText(tr(entry.text));
        }
    }

    sort_menu_actions_by_label(deps_.menus.language_menu);

    if (deps_.category_language.actions) {
        for (std::size_t idx = 0; idx < deps_.category_language.actions->size(); ++idx) {
            QAction* const action = deps_.category_language.actions->at(idx);
            if (!action) {
                continue;
            }
            const auto language = static_cast<CategoryLanguage>(idx);
            if (QAction* const shared_action =
                    shared_interface_language_action(deps_, language)) {
                action->setText(MenuMnemonicController::strip_mnemonic_markers(shared_action->text()));
                continue;
            }
            const QByteArray label_bytes = categoryLanguageToString(language).toUtf8();
            action->setText(tr(label_bytes.constData()));
        }
    }

    if (auto* menu = deps_.menus.help_menu) {
        const QString help_title = QString(QChar(0x200B)) + tr(kMenuTitleHelp);
        MenuMnemonicController::set_mnemonic_title(menu, help_title);
    }

    if (auto* dock = raw_ptr(deps_.file_explorer_dock)) {
        dock->setWindowTitle(tr(kDockTitleFileExplorer));
    }
}

void UiTranslator::translate_status_messages(const State& state) const
{
    QStatusBar* bar = deps_.window.statusBar();
    if (!bar) {
        return;
    }

    if (state.analysis_in_progress) {
        if (state.stop_analysis_requested) {
            bar->showMessage(tr("Cancelling analysis…"), 4000);
        } else {
            bar->showMessage(tr("Analyzing…"));
        }
    } else if (state.status_is_ready) {
        bar->showMessage(tr("Ready"));
    }
}

void UiTranslator::update_language_checks() const
{
    Language configured = deps_.settings.get_language();
    update_language_group_checks(configured);

    CategoryLanguage cat_lang = deps_.settings.get_category_language();
    update_category_language_checks(cat_lang);
}

void UiTranslator::update_language_group_checks(Language configured) const
{
    if (!deps_.language.language_group) {
        return;
    }
    QSignalBlocker blocker(deps_.language.language_group);
    if (deps_.language.english_action) {
        deps_.language.english_action->setChecked(configured == Language::English);
    }
    if (deps_.language.dutch_action) {
        deps_.language.dutch_action->setChecked(configured == Language::Dutch);
    }
    if (deps_.language.french_action) {
        deps_.language.french_action->setChecked(configured == Language::French);
    }
    if (deps_.language.german_action) {
        deps_.language.german_action->setChecked(configured == Language::German);
    }
    if (deps_.language.hindi_action) {
        deps_.language.hindi_action->setChecked(configured == Language::Hindi);
    }
    if (deps_.language.italian_action) {
        deps_.language.italian_action->setChecked(configured == Language::Italian);
    }
    if (deps_.language.simplified_chinese_action) {
        deps_.language.simplified_chinese_action->setChecked(
            configured == Language::SimplifiedChinese);
    }
    if (deps_.language.swedish_action) {
        deps_.language.swedish_action->setChecked(configured == Language::Swedish);
    }
    if (deps_.language.icelandic_action) {
        deps_.language.icelandic_action->setChecked(configured == Language::Icelandic);
    }
    if (deps_.language.norwegian_action) {
        deps_.language.norwegian_action->setChecked(configured == Language::Norwegian);
    }
    if (deps_.language.finnish_action) {
        deps_.language.finnish_action->setChecked(configured == Language::Finnish);
    }
    if (deps_.language.danish_action) {
        deps_.language.danish_action->setChecked(configured == Language::Danish);
    }
    if (deps_.language.spanish_action) {
        deps_.language.spanish_action->setChecked(configured == Language::Spanish);
    }
    if (deps_.language.turkish_action) {
        deps_.language.turkish_action->setChecked(configured == Language::Turkish);
    }
    if (deps_.language.korean_action) {
        deps_.language.korean_action->setChecked(configured == Language::Korean);
    }
}

void UiTranslator::update_category_language_checks(CategoryLanguage configured) const
{
    if (!deps_.category_language.category_language_group ||
        !deps_.category_language.actions) {
        return;
    }
    QSignalBlocker blocker_cat(deps_.category_language.category_language_group);
    for (std::size_t idx = 0; idx < deps_.category_language.actions->size(); ++idx) {
        QAction* const action = deps_.category_language.actions->at(idx);
        if (!action) {
            continue;
        }
        action->setChecked(idx == categoryLanguageIndex(configured));
    }
}

QString UiTranslator::tr(const char* source) const
{
    if (deps_.translator) {
        return deps_.translator(source);
    }
    return QCoreApplication::translate("UiTranslator", source);
}
