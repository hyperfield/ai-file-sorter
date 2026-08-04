# 2026-01-10: feat(rename): avoid reprocessing already-renamed pictures

## Covered commits
- `9d2d7a9` `2026-01-10` `feat(rename): avoid reprocessing already-renamed pictures`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationDialog.hpp`
- `M` `app/include/DatabaseManager.hpp`
- `M` `app/include/MainAppTestAccess.hpp`
- `M` `app/include/Types.hpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `tests/unit/test_categorization_dialog.cpp`
- `M` `tests/unit/test_database_manager_rename_only.cpp`
- `M` `tests/unit/test_main_app_image_options.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/include/CategorizationDialog.hpp`, `app/include/DatabaseManager.hpp`, `app/include/MainAppTestAccess.hpp`, `app/include/Types.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/DatabaseManager.cpp`, `app/lib/MainApp.cpp`, `tests/unit/test_categorization_dialog.cpp`, and 2 more file(s). It changed the project from not having the capability described by `feat(rename): avoid reprocessing already-renamed pictures` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `9d2d7a9`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -61,6 +61,8 @@ private:
     static constexpr int kUsedConsistencyRole = Qt::UserRole + 2;
     static constexpr int kRenameOnlyRole = Qt::UserRole + 3;
     static constexpr int kFileTypeRole = Qt::UserRole + 4;
+    static constexpr int kRenameAppliedRole = Qt::UserRole + 5;
+    static constexpr int kRenameLockedRole = Qt::UserRole + 6;
 
     enum Column {
         ColumnSelect = 0,
@@ -93,6 +95,7 @@ private:
 
     void setup_ui();
     void populate_model();
+    void ensure_unique_suggested_names_in_model();
     void record_categorization_to_db();
     void on_confirm_and_sort_button_clicked();
     void on_continue_later_button_clicked();
```

The excerpt is taken from the commit diff for `feat(rename): avoid reprocessing already-renamed pictures`. The most relevant surfaces are `app/include/CategorizationDialog.hpp`, `app/include/DatabaseManager.hpp`, `app/include/MainAppTestAccess.hpp`, `app/include/Types.hpp`, `app/lib/CategorizationDialog.cpp`, and 5 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
