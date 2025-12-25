#include "MainAppUiBuilder.hpp"

#include "MainApp.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "UiTranslator.hpp"
#include "Language.hpp"
#include "CategoryLanguage.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QDir>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QComboBox>
#include <QFontMetrics>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QPainter>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

void MainAppUiBuilder::build(MainApp& app) {
    build_central_panel(app);
    build_menus(app);
    app.analysis_in_progress_ = false;
    app.status_is_ready_ = true;
}

void MainAppUiBuilder::build_central_panel(MainApp& app) {
    app.setWindowTitle(QStringLiteral("AI File Sorter"));
    app.resize(1000, 800);

    QWidget* central = new QWidget(&app);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    auto* path_layout = new QHBoxLayout();
    app.path_label = new QLabel(central);
    app.path_entry = new QLineEdit(central);
    app.browse_button = new QPushButton(central);
    path_layout->addWidget(app.path_label);
    path_layout->addWidget(app.path_entry, 1);
    path_layout->addWidget(app.browse_button);
    main_layout->addLayout(path_layout);

    auto* options_layout = new QHBoxLayout();
    app.use_subcategories_checkbox = new QCheckBox(central);
    app.categorize_files_checkbox = new QCheckBox(central);
    app.categorize_directories_checkbox = new QCheckBox(central);
    app.categorize_files_checkbox->setChecked(true);
    options_layout->addWidget(app.use_subcategories_checkbox);
    options_layout->addWidget(app.categorize_files_checkbox);
    options_layout->addWidget(app.categorize_directories_checkbox);
    options_layout->addStretch(1);
    main_layout->addLayout(options_layout);

    app.categorization_style_heading = new QLabel(central);
    app.categorization_style_refined_radio = new QRadioButton(central);
    app.categorization_style_consistent_radio = new QRadioButton(central);
    app.use_whitelist_checkbox = new QCheckBox(central);
    app.whitelist_selector = new QComboBox(central);
    app.whitelist_selector->setEnabled(false);
    app.whitelist_selector->setMinimumContentsLength(16);
    app.whitelist_selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QFontMetrics fm(app.whitelist_selector->font());
    app.whitelist_selector->setMinimumWidth(fm.horizontalAdvance(QString(16, QChar('W'))) + 5);
    
    // Add context input field
    app.context_input = new QLineEdit(central);
    app.context_input->setPlaceholderText("Add context to help categorize (e.g., 'VST plugins', 'work documents')");
    app.context_input->setMaxLength(500);

    app.analyze_button = new QPushButton(central);
    QIcon analyze_icon = QIcon::fromTheme(QStringLiteral("sparkle"));
    if (analyze_icon.isNull()) {
        analyze_icon = QIcon::fromTheme(QStringLiteral("applications-education"));
    }
    if (analyze_icon.isNull()) {
        analyze_icon = app.style()->standardIcon(QStyle::SP_MediaPlay);
    }
    app.analyze_button->setIcon(analyze_icon);
    app.analyze_button->setIconSize(QSize(20, 20));
    app.analyze_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    app.analyze_button->setMinimumWidth(160);
    auto* analyze_layout = new QHBoxLayout();
    auto* categorization_layout = new QVBoxLayout();
    auto* toggle_row = new QHBoxLayout();
    toggle_row->addWidget(app.categorization_style_refined_radio);
    toggle_row->addWidget(app.categorization_style_consistent_radio);
    toggle_row->addStretch();
    categorization_layout->addWidget(app.categorization_style_heading);
    categorization_layout->addLayout(toggle_row);

    auto* whitelist_row = new QHBoxLayout();
    whitelist_row->addWidget(app.use_whitelist_checkbox);
    whitelist_row->addWidget(app.whitelist_selector);
    whitelist_row->addStretch();
    
    auto* context_row = new QHBoxLayout();
    context_row->addWidget(app.context_input);

    auto* control_block = new QVBoxLayout();
    control_block->addLayout(categorization_layout);
    control_block->addSpacing(4);
    control_block->addLayout(whitelist_row);
    control_block->addSpacing(4);
    control_block->addLayout(context_row);

    analyze_layout->addLayout(control_block);
    analyze_layout->addSpacing(12);
    analyze_layout->addWidget(app.analyze_button, 0, Qt::AlignBottom | Qt::AlignRight);
    main_layout->addLayout(analyze_layout);

    app.tree_model = new QStandardItemModel(0, 5, &app);

    app.results_stack = new QStackedWidget(central);

    app.tree_view = new QTreeView(app.results_stack);
    app.tree_view->setModel(app.tree_model);
    app.tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    app.tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    app.tree_view->header()->setSectionResizeMode(QHeaderView::Stretch);
    app.tree_view->setUniformRowHeights(true);
    app.tree_view_page_index_ = app.results_stack->addWidget(app.tree_view);

    app.folder_contents_model = new QFileSystemModel(app.results_stack);
    app.folder_contents_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    app.folder_contents_model->setRootPath(QDir::homePath());

    app.folder_contents_view = new QTreeView(app.results_stack);
    app.folder_contents_view->setModel(app.folder_contents_model);
    app.folder_contents_view->setRootIndex(app.folder_contents_model->index(QDir::homePath()));
    app.folder_contents_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    app.folder_contents_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    app.folder_contents_view->setRootIsDecorated(false);
    app.folder_contents_view->setUniformRowHeights(true);
    app.folder_contents_view->setSortingEnabled(true);
    app.folder_contents_view->sortByColumn(0, Qt::AscendingOrder);
    app.folder_contents_view->setAlternatingRowColors(true);
    app.folder_view_page_index_ = app.results_stack->addWidget(app.folder_contents_view);

    app.results_stack->setCurrentIndex(app.tree_view_page_index_);
    main_layout->addWidget(app.results_stack, 1);

    app.setCentralWidget(central);
}

UiTranslator::Dependencies MainAppUiBuilder::build_translator_dependencies(MainApp& app) const
{
    return UiTranslator::Dependencies{
        .window = app,
        .primary = UiTranslator::PrimaryControls{
            app.path_label,
            app.browse_button,
            app.analyze_button,
            app.use_subcategories_checkbox,
            app.categorization_style_heading,
            app.categorization_style_refined_radio,
            app.categorization_style_consistent_radio,
            app.use_whitelist_checkbox,
            app.whitelist_selector,
            app.categorize_files_checkbox,
            app.categorize_directories_checkbox},
        .tree_model = app.tree_model,
        .menus = UiTranslator::MenuControls{
            app.file_menu,
            app.edit_menu,
            app.view_menu,
            app.settings_menu,
            app.development_menu,
            app.development_settings_menu,
            app.language_menu,
            app.category_language_menu,
            app.help_menu},
        .actions = UiTranslator::ActionControls{
            app.file_quit_action,
            app.copy_action,
            app.cut_action,
            app.undo_last_run_action,
            app.paste_action,
            app.delete_action,
            app.toggle_explorer_action,
            app.toggle_llm_action,
            app.manage_whitelists_action,
            app.development_prompt_logging_action,
            app.consistency_pass_action,
            app.english_action,
            app.french_action,
            app.german_action,
            app.italian_action,
            app.spanish_action,
            app.turkish_action,
            app.category_language_english,
            app.category_language_french,
            app.category_language_german,
            app.category_language_italian,
            app.category_language_dutch,
            app.category_language_polish,
            app.category_language_portuguese,
            app.category_language_spanish,
            app.category_language_turkish,
            app.about_action,
            app.about_qt_action,
            app.about_agpl_action,
            app.support_project_action},
        .language = UiTranslator::LanguageControls{
            app.language_group,
            app.english_action,
            app.french_action,
            app.german_action,
            app.italian_action,
            app.spanish_action,
            app.turkish_action},
        .category_language = UiTranslator::CategoryLanguageControls{
            app.category_language_group,
            app.category_language_dutch,
            app.category_language_english,
            app.category_language_french,
            app.category_language_german,
            app.category_language_italian,
            app.category_language_polish,
            app.category_language_portuguese,
            app.category_language_spanish,
            app.category_language_turkish},
        .file_explorer_dock = app.file_explorer_dock,
        .settings = app.settings,
        .translator = [](const char* source) {
            return MainApp::tr(source);
        }};
}

void MainAppUiBuilder::build_menus(MainApp& app) {
    build_file_menu(app);
    build_edit_menu(app);
    build_view_menu(app);
    build_settings_menu(app);
    if (app.is_development_mode()) {
        build_development_menu(app);
    }
    build_help_menu(app);
}

void MainAppUiBuilder::build_file_menu(MainApp& app) {
    app.file_menu = app.menuBar()->addMenu(QString());
    app.file_quit_action = app.file_menu->addAction(icon_for(app, "application-exit", QStyle::SP_DialogCloseButton), QString());
    app.file_quit_action->setShortcut(QKeySequence::Quit);
    app.file_quit_action->setMenuRole(QAction::QuitRole);
    QObject::connect(app.file_quit_action, &QAction::triggered, qApp, &QApplication::quit);
}

void MainAppUiBuilder::build_edit_menu(MainApp& app) {
    app.edit_menu = app.menuBar()->addMenu(QString());

    app.undo_last_run_action = app.edit_menu->addAction(icon_for(app, "edit-undo", QStyle::SP_ArrowBack), QString());
    QObject::connect(app.undo_last_run_action, &QAction::triggered, &app, &MainApp::undo_last_run);

    app.copy_action = app.edit_menu->addAction(icon_for(app, "edit-copy", QStyle::SP_FileDialogContentsView), QString());
    QObject::connect(app.copy_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_copy(app.path_entry);
    });
    app.copy_action->setShortcut(QKeySequence::Copy);

    app.cut_action = app.edit_menu->addAction(icon_for(app, "edit-cut", QStyle::SP_FileDialogDetailedView), QString());
    QObject::connect(app.cut_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_cut(app.path_entry);
    });
    app.cut_action->setShortcut(QKeySequence::Cut);

    app.paste_action = app.edit_menu->addAction(icon_for(app, "edit-paste", QStyle::SP_FileDialogListView), QString());
    QObject::connect(app.paste_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_paste(app.path_entry);
    });
    app.paste_action->setShortcut(QKeySequence::Paste);

    app.delete_action = app.edit_menu->addAction(icon_for(app, "edit-delete", QStyle::SP_TrashIcon), QString());
    QObject::connect(app.delete_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_delete(app.path_entry);
    });
    app.delete_action->setShortcut(QKeySequence::Delete);
}

void MainAppUiBuilder::build_view_menu(MainApp& app) {
    app.view_menu = app.menuBar()->addMenu(QString());
    app.toggle_explorer_action = app.view_menu->addAction(icon_for(app, "system-file-manager", QStyle::SP_DirOpenIcon), QString());
    app.toggle_explorer_action->setCheckable(true);
    app.toggle_explorer_action->setChecked(app.settings.get_show_file_explorer());
    QObject::connect(app.toggle_explorer_action, &QAction::toggled, &app, [&app](bool checked) {
        if (app.file_explorer_dock) {
            app.file_explorer_dock->setVisible(checked);
        }
        app.settings.set_show_file_explorer(checked);
        app.update_results_view_mode();
    });
    app.file_explorer_menu_action = app.toggle_explorer_action;
}

void MainAppUiBuilder::build_settings_menu(MainApp& app) {
    app.settings_menu = app.menuBar()->addMenu(QString());
    app.toggle_llm_action = app.settings_menu->addAction(icon_for(app, "preferences-system", QStyle::SP_DialogApplyButton), QString());
    QObject::connect(app.toggle_llm_action, &QAction::triggered, &app, &MainApp::show_llm_selection_dialog);

    app.manage_whitelists_action = app.settings_menu->addAction(QString());
    QObject::connect(app.manage_whitelists_action, &QAction::triggered, &app, &MainApp::show_whitelist_manager);

    app.language_menu = app.settings_menu->addMenu(QString());
    app.language_group = new QActionGroup(&app);
    app.language_group->setExclusive(true);

    app.english_action = app.language_menu->addAction(QString());
    app.english_action->setCheckable(true);
    app.english_action->setData(static_cast<int>(Language::English));
    app.language_group->addAction(app.english_action);

    app.french_action = app.language_menu->addAction(QString());
    app.french_action->setCheckable(true);
    app.french_action->setData(static_cast<int>(Language::French));
    app.language_group->addAction(app.french_action);
    app.german_action = app.language_menu->addAction(QString());
    app.german_action->setCheckable(true);
    app.german_action->setData(static_cast<int>(Language::German));
    app.language_group->addAction(app.german_action);
    app.italian_action = app.language_menu->addAction(QString());
    app.italian_action->setCheckable(true);
    app.italian_action->setData(static_cast<int>(Language::Italian));
    app.language_group->addAction(app.italian_action);
    app.spanish_action = app.language_menu->addAction(QString());
    app.spanish_action->setCheckable(true);
    app.spanish_action->setData(static_cast<int>(Language::Spanish));
    app.language_group->addAction(app.spanish_action);
    app.turkish_action = app.language_menu->addAction(QString());
    app.turkish_action->setCheckable(true);
    app.turkish_action->setData(static_cast<int>(Language::Turkish));
    app.language_group->addAction(app.turkish_action);

    QObject::connect(app.language_group, &QActionGroup::triggered, &app, [&app](QAction* action) {
        if (!action) {
            return;
        }
        const Language chosen = static_cast<Language>(action->data().toInt());
        app.on_language_selected(chosen);
    });

    app.category_language_menu = app.settings_menu->addMenu(QString());
    app.category_language_group = new QActionGroup(&app);
    app.category_language_group->setExclusive(true);

    const auto add_cat_lang = [&](CategoryLanguage lang) {
        QAction* act = app.category_language_menu->addAction(QString());
        act->setCheckable(true);
        act->setData(static_cast<int>(lang));
        app.category_language_group->addAction(act);
        return act;
    };
    app.category_language_dutch = add_cat_lang(CategoryLanguage::Dutch);
    app.category_language_english = add_cat_lang(CategoryLanguage::English);
    app.category_language_french = add_cat_lang(CategoryLanguage::French);
    app.category_language_german = add_cat_lang(CategoryLanguage::German);
    app.category_language_italian = add_cat_lang(CategoryLanguage::Italian);
    app.category_language_polish = add_cat_lang(CategoryLanguage::Polish);
    app.category_language_portuguese = add_cat_lang(CategoryLanguage::Portuguese);
    app.category_language_spanish = add_cat_lang(CategoryLanguage::Spanish);
    app.category_language_turkish = add_cat_lang(CategoryLanguage::Turkish);

    QObject::connect(app.category_language_group, &QActionGroup::triggered, &app, [&app](QAction* action) {
        if (!action) {
            return;
        }
        const CategoryLanguage chosen = static_cast<CategoryLanguage>(action->data().toInt());
        app.on_category_language_selected(chosen);
    });
}

void MainAppUiBuilder::build_development_menu(MainApp& app) {
    app.development_menu = app.menuBar()->addMenu(QString());
    app.development_settings_menu = app.development_menu->addMenu(QString());
    app.development_prompt_logging_action = app.development_settings_menu->addAction(QString());
    app.development_prompt_logging_action->setCheckable(true);
    app.development_prompt_logging_action->setChecked(app.development_prompt_logging_enabled_);
    QObject::connect(app.development_prompt_logging_action, &QAction::toggled, &app, &MainApp::handle_development_prompt_logging);
}

void MainAppUiBuilder::build_help_menu(MainApp& app) {
    app.help_menu = app.menuBar()->addMenu(QString());
    if (app.help_menu && app.help_menu->menuAction()) {
        app.help_menu->menuAction()->setMenuRole(QAction::ApplicationSpecificRole);
    }

    app.about_action = app.help_menu->addAction(icon_for(app, "help-about", QStyle::SP_MessageBoxInformation), QString());
    app.about_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.about_action, &QAction::triggered, &app, &MainApp::on_about_activate);

    app.about_qt_action = app.help_menu->addAction(icon_for(app, "help-about", QStyle::SP_MessageBoxInformation), QString());
    app.about_qt_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.about_qt_action, &QAction::triggered, &app, [&app]() {
        QMessageBox::aboutQt(&app);
    });

    app.about_agpl_action = app.help_menu->addAction(icon_for(app, "help-about", QStyle::SP_MessageBoxInformation), QString());
    app.about_agpl_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.about_agpl_action, &QAction::triggered, &app, [&app]() {
        MainAppHelpActions::show_agpl_info(&app);
    });

    app.support_project_action = app.help_menu->addAction(icon_for(app, "help-donate", QStyle::SP_DialogHelpButton), QString());
    app.support_project_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.support_project_action, &QAction::triggered, &app, []() {
        MainAppHelpActions::open_support_page();
    });
}

QIcon MainAppUiBuilder::icon_for(MainApp& app, const char* name, QStyle::StandardPixmap fallback) {
    QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
    if (icon.isNull()) {
        icon = app.style()->standardIcon(fallback);
    }
    if (!icon.isNull()) {
        const int targetSize = app.style()->pixelMetric(QStyle::PM_SmallIconSize);
        if (targetSize > 0) {
            QPixmap pixmap = icon.pixmap(targetSize, targetSize);
            if (!pixmap.isNull()) {
                const int padding = std::max(4, targetSize / 4);
                const QSize paddedSize(pixmap.width() + padding * 2, pixmap.height() + padding * 2);
                QPixmap padded(paddedSize);
                padded.fill(Qt::transparent);
                QPainter painter(&padded);
                painter.drawPixmap(padding, padding, pixmap);
                painter.end();
                QIcon paddedIcon;
                paddedIcon.addPixmap(padded, QIcon::Normal);
                paddedIcon.addPixmap(padded, QIcon::Disabled);
                icon = paddedIcon;
            }
        }
    }
    return icon;
}
