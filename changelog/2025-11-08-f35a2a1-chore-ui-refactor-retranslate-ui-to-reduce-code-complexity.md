# 2025-11-08: chore(ui): refactor retranslate_ui() to reduce code complexity

## Covered commits
- `f35a2a1` `2025-11-08` `chore(ui): refactor retranslate_ui() to reduce code complexity`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/MainApp.hpp`, `app/lib/MainApp.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `f35a2a1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -82,6 +82,11 @@ private:
     void sync_settings_to_ui();
     void sync_ui_to_settings();
     void retranslate_ui();
+    void translate_window_title();
+    void translate_primary_controls();
+    void translate_tree_view_labels();
+    void translate_menus_and_actions();
+    void translate_status_messages();
     void update_language_checks();
     void on_language_selected(Language language);
 
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
index f97ac36..bcd0349 100644
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -343,9 +343,22 @@ void MainApp::sync_ui_to_settings()
 }
 
 void MainApp::retranslate_ui()
+{
+    translate_window_title();
+    translate_primary_controls();
+    translate_tree_view_labels();
+    translate_menus_and_actions();
+    translate_status_messages();
+    update_language_checks();
+}
+
+void MainApp::translate_window_title()
 {
     setWindowTitle(QStringLiteral("AI File Sorter"));
+}
 
+void MainApp::translate_primary_controls()
+{
     if (path_label) {
         path_label->setText(tr("Folder:"));
     }
```

The excerpt is taken from the commit diff for `chore(ui): refactor retranslate_ui() to reduce code complexity`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
