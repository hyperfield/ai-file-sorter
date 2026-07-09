# 2025-10-27: fix(menu-bar): on macOS

## Covered commits
- `adc7d5d` `2025-10-27` `fix(menu-bar): on macOS`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/MainApp.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(menu-bar): on macOS`.

Before this commit, the repository reflected the state immediately preceding `adc7d5d`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -224,8 +224,6 @@ void MainApp::setup_ui()
 
     setCentralWidget(central);
 
-    menuBar()->setNativeMenuBar(false);
-
     setup_menus();
     analysis_in_progress_ = false;
     status_is_ready_ = true;
@@ -288,7 +286,6 @@ void MainApp::setup_menus()
 
     settings_menu = menuBar()->addMenu(QString());
     toggle_llm_action = settings_menu->addAction(themed_icon("preferences-system", QStyle::SP_DialogApplyButton), QString());
-    toggle_llm_action->setMenuRole(QAction::PreferencesRole);
     connect(toggle_llm_action, &QAction::triggered, this, &MainApp::show_llm_selection_dialog);
 
     language_menu = settings_menu->addMenu(QString());
```

The excerpt is taken from the commit diff for `fix(menu-bar): on macOS`. The most relevant surfaces are `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
