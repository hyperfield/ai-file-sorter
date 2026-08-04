# 2026-02-01: feat(cache): persist LLM outputs during analysis

## Covered commits
- `c79e7a5` `2026-02-01` `feat(cache): persist LLM outputs during analysis`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `tests/unit/test_database_manager_rename_only.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/lib/DatabaseManager.cpp`, `app/lib/MainApp.cpp`, `tests/unit/test_database_manager_rename_only.cpp`. It changed the project from not having the capability described by `feat(cache): persist LLM outputs during analysis` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `c79e7a5`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/DatabaseManager.cpp b/app/lib/DatabaseManager.cpp
--- a/app/lib/DatabaseManager.cpp
+++ b/app/lib/DatabaseManager.cpp
@@ -152,7 +152,9 @@ std::optional<CategorizedFile> build_categorized_entry(sqlite3_stmt* stmt) {
         rename_applied = sqlite3_column_int(stmt, 9) != 0;
     }
 
-    if (!rename_only && (!has_label_content(cat) || !has_label_content(subcat))) {
+    const bool has_labels = has_label_content(cat) && has_label_content(subcat);
+    const bool has_suggestion = has_label_content(suggested);
+    if (!rename_only && !has_labels && !has_suggestion) {
         return std::nullopt;
     }
 
@@ -878,6 +880,7 @@ DatabaseManager::remove_empty_categorizations(const std::string& dir_path) {
         FROM file_categorization
         WHERE dir_path = ?
           AND (category IS NULL OR TRIM(category) = '' OR subcategory IS NULL OR TRIM(subcategory) = '')
+          AND (suggested_name IS NULL OR TRIM(suggested_name) = '')
           AND IFNULL(rename_only, 0) = 0;
     )";
```

The excerpt is taken from the commit diff for `feat(cache): persist LLM outputs during analysis`. The most relevant surfaces are `app/lib/DatabaseManager.cpp`, `app/lib/MainApp.cpp`, `tests/unit/test_database_manager_rename_only.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
