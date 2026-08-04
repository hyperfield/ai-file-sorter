# 2025-08-06: chore(categorization-dialog): add logger

## Covered commits
- `5dd7034` `2025-08-06` `chore(categorization-dialog): add logger`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationDialog.hpp`
- `M` `app/lib/CategorizationDialog.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/CategorizationDialog.hpp`, `app/lib/CategorizationDialog.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(categorization-dialog): add logger`.

Before this commit, the repository reflected the state immediately preceding `5dd7034`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -14,6 +14,9 @@ public:
     void on_confirm_and_sort_button_clicked();
 
 private:
+    std::shared_ptr<spdlog::logger> core_logger;
+    std::shared_ptr<spdlog::logger> ui_logger;
+    std::shared_ptr<spdlog::logger> db_logger;
     GtkDialog *dialog;
     GtkButton *confirm_button;
     GtkButton *continue_button;
diff --git a/app/lib/CategorizationDialog.cpp b/app/lib/CategorizationDialog.cpp
index 4b5d67f..572a9b7 100644
--- a/app/lib/CategorizationDialog.cpp
+++ b/app/lib/CategorizationDialog.cpp
@@ -9,19 +9,23 @@
 #include <MovableCategorizedFile.hpp>
 #include <DatabaseManager.hpp>
 #include <Types.hpp>
+#include <Logger.hpp>
 
 
 CategorizationDialog::CategorizationDialog(DatabaseManager* db_manager, gboolean show_subcategory_col)
-    : db_manager(db_manager), show_subcategory_col(show_subcategory_col)
+    : db_manager(db_manager), show_subcategory_col(show_subcategory_col),
+      core_logger(Logger::get_logger("core_logger")),
+      db_logger(Logger::get_logger("db_logger")),
+      ui_logger(Logger::get_logger("ui_logger"))
 {
     builder = gtk_builder_new();
     if (!builder) {
-        g_critical("Failed to initialize GtkBuilder.");
+        ui_logger->critical("Failed to initialize GtkBuilder.");
         return;
     }
     GError *error = NULL;
     if (!gtk_builder_add_from_resource(builder, "/net/quicknode/AIFileSorter/ui/sort_confirm.glade", &error)) {
-        g_critical("Failed to load resource: %s", error->message);
+        ui_logger->critical("Failed to load resource: %s", error->message);
         g_error_free(error);
         if (builder) {
             g_object_unref(builder);
```

The excerpt is taken from the commit diff for `chore(categorization-dialog): add logger`. The most relevant surfaces are `app/include/CategorizationDialog.hpp`, `app/lib/CategorizationDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
