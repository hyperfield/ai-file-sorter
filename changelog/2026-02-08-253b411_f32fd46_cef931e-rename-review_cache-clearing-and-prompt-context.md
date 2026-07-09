# 2026-02-08: Rename review correctness, cache clearing, and prompt-context enrichment

## Covered commits
- `253b411` `2026-02-08` `fix(review-dialog): keep original filenames after rename`
- `f32fd46` `2026-02-08` `fix(cache): use selected folder when clearing cache`
- `cef931e` `2026-02-08` `feat(prompt): always include user context`
- `95d1709` `2026-02-08` `fix(build): remove Q_OBJECT from simple dialogs`
- `4a97d03` `2026-02-08` `fix(types): include vector for profile structs`

## Motivation
After the rename-only and document/image flows expanded, the review dialog and cache-management paths needed cleanup so they preserved user expectations. Prompt quality also benefitted from always including the current user context instead of making that optional or inconsistent.

## What changed
The grouped changes preserved original filenames after rename, fixed cache clearing to target the selected folder correctly, always included user context in prompts, and cleaned up minor build issues such as unnecessary `Q_OBJECT` use and missing includes.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `253b411`
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -69,6 +69,7 @@ private:
     static constexpr int kRenameLockedRole = Qt::UserRole + 6;
     static constexpr int kHiddenCategoryRole = Qt::UserRole + 7;
     static constexpr int kHiddenSubcategoryRole = Qt::UserRole + 8;
+    static constexpr int kOriginalFileNameRole = Qt::UserRole + 9;
 
     enum Column {
         ColumnSelect = 0,
diff --git a/app/lib/CategorizationDialog.cpp b/app/lib/CategorizationDialog.cpp
index 28ba1a9..f840f51 100644
--- a/app/lib/CategorizationDialog.cpp
+++ b/app/lib/CategorizationDialog.cpp
@@ -1055,6 +1055,7 @@ void CategorizationDialog::populate_model()
         auto* file_item = new QStandardItem(QString::fromStdString(file.file_name));
         file_item->setEditable(false);
         file_item->setData(QString::fromStdString(file.file_path), kFilePathRole);
+        file_item->setData(QString::fromStdString(file.file_name), kOriginalFileNameRole);
         file_item->setData(file.used_consistency_hints, kUsedConsistencyRole);
         file_item->setData(file.rename_only, kRenameOnlyRole);
         file_item->setData(static_cast<int>(file.type), kFileTypeRole);
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `f32fd46`
```diff
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -3686,18 +3686,17 @@ void MainApp::clear_categorization_cache()
     
     if (confirm_dialog.clickedButton() == current_folder_button) {
         // Clear cache for current folder only
-        QString folder_path = folder_path_input->text();
-        if (folder_path.isEmpty()) {
+        const std::string folder_path = get_folder_path();
+        if (folder_path.empty()) {
             show_error_dialog(tr("Please select a folder first.").toStdString());
             return;
         }
-        
-        success = db_manager.clear_directory_categorizations(folder_path.toStdString());
+
+        success = db_manager.clear_directory_categorizations(folder_path);
         if (success) {
             status_message = tr("Cache cleared for current folder.");
             if (core_logger) {
-                core_logger->info("Cleared categorization cache for folder: {}", 
-                                 folder_path.toStdString());
+                core_logger->info("Cleared categorization cache for folder: {}", folder_path);
             }
         } else {
             status_message = tr("Failed to clear cache for current folder.");
```

This second excerpt is included because `f32fd46` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
