# 2025-11-01: chore(mainapp): code refactor to reduce complexity

## Covered commits
- `3127185` `2025-11-01` `chore(mainapp): code refactor to reduce complexity`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `A` `app/include/MainAppUiBuilder.hpp`
- `M` `app/lib/MainApp.cpp`
- `A` `app/lib/MainAppUiBuilder.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/MainApp.hpp`, `app/include/MainAppUiBuilder.hpp`, `app/lib/MainApp.cpp`, `app/lib/MainAppUiBuilder.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `3127185`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -38,6 +38,7 @@ class QStackedWidget;
 class QWidget;
 class QLabel;
 class QEvent;
+class MainAppUiBuilder;
 
 struct CategorizedFile;
 struct FileEntry;
@@ -64,8 +65,6 @@ protected:
     void closeEvent(QCloseEvent* event) override;
 
 private:
-    void setup_ui();
-    void setup_menus();
     void setup_file_explorer();
     void connect_signals();
     void connect_edit_actions();
```

The excerpt is taken from the commit diff for `chore(mainapp): code refactor to reduce complexity`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/include/MainAppUiBuilder.hpp`, `app/lib/MainApp.cpp`, `app/lib/MainAppUiBuilder.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
