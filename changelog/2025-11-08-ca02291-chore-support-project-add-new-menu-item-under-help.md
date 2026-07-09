# 2025-11-08: chore(support-project): add new menu item under 'Help'

## Covered commits
- `ca02291` `2025-11-08` `chore(support-project): add new menu item under 'Help'`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/include/MainAppHelpActions.hpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MainAppHelpActions.cpp`
- `M` `app/lib/MainAppUiBuilder.cpp`
- `M` `app/lib/TranslationManager.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/MainApp.hpp`, `app/include/MainAppHelpActions.hpp`, `app/lib/MainApp.cpp`, `app/lib/MainAppHelpActions.cpp`, `app/lib/MainAppUiBuilder.cpp`, `app/lib/TranslationManager.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(support-project): add new menu item under 'Help'`.

Before this commit, the repository reflected the state immediately preceding `ca02291`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -164,6 +164,7 @@ private:
     QAction* about_action{nullptr};
     QAction* about_qt_action{nullptr};
     QAction* about_agpl_action{nullptr};
+    QAction* support_project_action{nullptr};
 
     std::unique_ptr<CategorizationDialog> categorization_dialog;
     std::unique_ptr<CategorizationProgressDialog> progress_dialog;
diff --git a/app/include/MainAppHelpActions.hpp b/app/include/MainAppHelpActions.hpp
index 66ffcf5..775c804 100644
--- a/app/include/MainAppHelpActions.hpp
+++ b/app/include/MainAppHelpActions.hpp
@@ -7,6 +7,7 @@ class MainAppHelpActions {
 public:
     static void show_about(QWidget* parent);
     static void show_agpl_info(QWidget* parent);
+    static void open_support_page();
 };
 
 #endif // MAIN_APP_HELP_ACTIONS_HPP
```

The excerpt is taken from the commit diff for `chore(support-project): add new menu item under 'Help'`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/include/MainAppHelpActions.hpp`, `app/lib/MainApp.cpp`, `app/lib/MainAppHelpActions.cpp`, `app/lib/MainAppUiBuilder.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
