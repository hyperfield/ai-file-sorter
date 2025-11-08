#include "MainAppUiBuilder.hpp"

#include "MainApp.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "Language.hpp"

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
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

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
    analyze_layout->addStretch();
    analyze_layout->addWidget(app.analyze_button);
    analyze_layout->addStretch();
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

    QObject::connect(app.language_group, &QActionGroup::triggered, &app, [&app](QAction* action) {
        if (!action) {
            return;
        }
        const Language chosen = static_cast<Language>(action->data().toInt());
        app.on_language_selected(chosen);
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
    return icon;
}
