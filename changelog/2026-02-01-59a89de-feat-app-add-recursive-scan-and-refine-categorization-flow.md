# 2026-02-01: feat(app): add recursive scan and refine categorization flow

## Covered commits
- `59a89de` `2026-02-01` `feat(app): add recursive scan and refine categorization flow`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationDialog.hpp`
- `M` `app/include/DatabaseManager.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/MovableCategorizedFile.hpp`
- `M` `app/include/ResultsCoordinator.hpp`
- `M` `app/include/Settings.hpp`
- `M` `app/include/Types.hpp`
- `M` `app/include/UiTranslator.hpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/CategorizationService.cpp`
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/FileScanner.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MainAppUiBuilder.cpp`
- `M` `app/lib/MovableCategorizedFile.cpp`
- `M` `app/lib/ResultsCoordinator.cpp`
- `M` `app/lib/Settings.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/include/CategorizationDialog.hpp`, `app/include/DatabaseManager.hpp`, `app/include/MainApp.hpp`, `app/include/MovableCategorizedFile.hpp`, `app/include/ResultsCoordinator.hpp`, `app/include/Settings.hpp`, `app/include/Types.hpp`, `app/include/UiTranslator.hpp`, and 9 more file(s). It changed the project from not having the capability described by `feat(app): add recursive scan and refine categorization flow` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `59a89de`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -39,7 +39,11 @@ public:
 #endif
 
     bool is_dialog_valid() const;
-    void show_results(const std::vector<CategorizedFile>& categorized_files);
+    void show_results(const std::vector<CategorizedFile>& categorized_files,
+                      const std::string& base_dir_override = std::string(),
+                      bool include_subdirectories = false,
+                      bool allow_image_renames = true,
+                      bool allow_document_renames = true);
 
 protected:
     void closeEvent(QCloseEvent* event) override;
@@ -138,6 +142,7 @@ private:
                              const std::string& rename_candidate,
                              const std::string& category,
                              const std::string& subcategory,
+                             const std::string& source_dir,
                              const std::string& base_dir,
                              std::vector<std::string>& files_not_moved,
                              FileType file_type,
```

The excerpt is taken from the commit diff for `feat(app): add recursive scan and refine categorization flow`. The most relevant surfaces are `app/include/CategorizationDialog.hpp`, `app/include/DatabaseManager.hpp`, `app/include/MainApp.hpp`, `app/include/MovableCategorizedFile.hpp`, `app/include/ResultsCoordinator.hpp`, and 12 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
