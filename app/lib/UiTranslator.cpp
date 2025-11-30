#include "UiTranslator.hpp"

#include "Language.hpp"
#include "CategoryLanguage.hpp"
#include "Settings.hpp"

#include <QAction>
#include <QActionGroup>
#include <QChar>
#include <QCheckBox>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringList>

namespace {

template <typename Widget>
Widget* raw_ptr(const QPointer<Widget>& pointer)
{
    return pointer.data();
}

} // namespace

UiTranslator::UiTranslator(Dependencies deps)
    : deps_(deps)
{
    if (!deps_.translator) {
        deps_.translator = [](const char* source) {
            return QObject::tr(source);
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
    }
    if (auto* heading = raw_ptr(deps_.primary.categorization_style_heading)) {
        heading->setText(tr("Categorization type"));
    }
    if (auto* refined_radio = raw_ptr(deps_.primary.categorization_style_refined_radio)) {
        refined_radio->setText(tr("More refined"));
    }
    if (auto* consistent_radio = raw_ptr(deps_.primary.categorization_style_consistent_radio)) {
        consistent_radio->setText(tr("More consistent"));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.use_whitelist_checkbox)) {
        checkbox->setText(tr("Use a whitelist"));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.categorize_files_checkbox)) {
        checkbox->setText(tr("Categorize files"));
    }
    if (auto* checkbox = raw_ptr(deps_.primary.categorize_directories_checkbox)) {
        checkbox->setText(tr("Categorize directories"));
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
    };

    const MenuEntry menu_entries[] = {
        {deps_.menus.file_menu, "&File"},
        {deps_.menus.edit_menu, "&Edit"},
        {deps_.menus.view_menu, "&View"},
        {deps_.menus.settings_menu, "&Settings"},
        {deps_.menus.development_menu, "&Development"},
        {deps_.menus.development_settings_menu, "&Settings"},
        {deps_.menus.language_menu, "Interface &language"},
        {deps_.menus.category_language_menu, "Category &language"}
    };

    for (const MenuEntry& entry : menu_entries) {
        if (entry.menu && entry.text) {
            entry.menu->setTitle(tr(entry.text));
        }
    }

    struct ActionEntry {
        QAction* action{nullptr};
        const char* text{nullptr};
    };

    const ActionEntry action_entries[] = {
        {deps_.actions.file_quit_action, "&Quit"},
        {deps_.actions.copy_action, "&Copy"},
        {deps_.actions.cut_action, "Cu&t"},
        {deps_.actions.undo_last_run_action, "Undo last run"},
        {deps_.actions.paste_action, "&Paste"},
        {deps_.actions.delete_action, "&Delete"},
        {deps_.actions.toggle_explorer_action, "File &Explorer"},
        {deps_.actions.toggle_llm_action, "Select &LLM…"},
        {deps_.actions.manage_whitelists_action, "Manage category whitelists…"},
        {deps_.actions.development_prompt_logging_action, "Log prompts and responses to stdout"},
        {deps_.actions.consistency_pass_action, "Run &consistency pass"},
        {deps_.actions.english_action, "&English"},
        {deps_.actions.french_action, "&French"},
        {deps_.actions.german_action, "&German"},
        {deps_.actions.italian_action, "&Italian"},
        {deps_.actions.spanish_action, "&Spanish"},
        {deps_.actions.turkish_action, "&Turkish"},
        {deps_.actions.category_language_dutch, "Dutch"},
        {deps_.actions.category_language_english, "English"},
        {deps_.actions.category_language_french, "French"},
        {deps_.actions.category_language_german, "German"},
        {deps_.actions.category_language_italian, "Italian"},
        {deps_.actions.category_language_polish, "Polish"},
        {deps_.actions.category_language_portuguese, "Portuguese"},
        {deps_.actions.category_language_spanish, "Spanish"},
        {deps_.actions.category_language_turkish, "Turkish"},
        {deps_.actions.about_action, "&About AI File Sorter"},
        {deps_.actions.about_qt_action, "About &Qt"},
        {deps_.actions.about_agpl_action, "About &AGPL"},
        {deps_.actions.support_project_action, "&Support Project"}
    };

    for (const ActionEntry& entry : action_entries) {
        if (entry.action && entry.text) {
            entry.action->setText(tr(entry.text));
        }
    }

    if (auto* menu = deps_.menus.help_menu) {
        const QString help_title = QString(QChar(0x200B)) + tr("&Help");
        menu->setTitle(help_title);
        if (QAction* help_action = menu->menuAction()) {
            help_action->setText(help_title);
        }
    }

    if (auto* dock = raw_ptr(deps_.file_explorer_dock)) {
        dock->setWindowTitle(tr("File Explorer"));
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
    if (deps_.language.french_action) {
        deps_.language.french_action->setChecked(configured == Language::French);
    }
    if (deps_.language.german_action) {
        deps_.language.german_action->setChecked(configured == Language::German);
    }
    if (deps_.language.italian_action) {
        deps_.language.italian_action->setChecked(configured == Language::Italian);
    }
    if (deps_.language.spanish_action) {
        deps_.language.spanish_action->setChecked(configured == Language::Spanish);
    }
    if (deps_.language.turkish_action) {
        deps_.language.turkish_action->setChecked(configured == Language::Turkish);
    }
}

void UiTranslator::update_category_language_checks(CategoryLanguage configured) const
{
    if (!deps_.category_language.category_language_group) {
        return;
    }
    QSignalBlocker blocker_cat(deps_.category_language.category_language_group);
    if (deps_.category_language.dutch) {
        deps_.category_language.dutch->setChecked(configured == CategoryLanguage::Dutch);
    }
    if (deps_.category_language.english) {
        deps_.category_language.english->setChecked(configured == CategoryLanguage::English);
    }
    if (deps_.category_language.french) {
        deps_.category_language.french->setChecked(configured == CategoryLanguage::French);
    }
    if (deps_.category_language.german) {
        deps_.category_language.german->setChecked(configured == CategoryLanguage::German);
    }
    if (deps_.category_language.italian) {
        deps_.category_language.italian->setChecked(configured == CategoryLanguage::Italian);
    }
    if (deps_.category_language.polish) {
        deps_.category_language.polish->setChecked(configured == CategoryLanguage::Polish);
    }
    if (deps_.category_language.portuguese) {
        deps_.category_language.portuguese->setChecked(configured == CategoryLanguage::Portuguese);
    }
    if (deps_.category_language.spanish) {
        deps_.category_language.spanish->setChecked(configured == CategoryLanguage::Spanish);
    }
    if (deps_.category_language.turkish) {
        deps_.category_language.turkish->setChecked(configured == CategoryLanguage::Turkish);
    }
}

QString UiTranslator::tr(const char* source) const
{
    if (deps_.translator) {
        return deps_.translator(source);
    }
    return QObject::tr(source);
}
