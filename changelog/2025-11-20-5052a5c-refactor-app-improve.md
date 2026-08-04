# 2025-11-20: refactor(app): improve

## Covered commits
- `5052a5c` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationDialog.hpp`
- `M` `app/include/MovableCategorizedFile.hpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MovableCategorizedFile.cpp`
- `M` `app/lib/TranslationManager.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/CategorizationDialog.hpp`, `app/include/MovableCategorizedFile.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MovableCategorizedFile.cpp`, `app/lib/TranslationManager.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `5052a5c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -70,14 +70,13 @@ private:
     void retranslate_ui();
     void apply_status_text(QStandardItem* item) const;
     RowStatus status_from_item(const QStandardItem* item) const;
-    std::vector<std::tuple<bool, std::string, std::string, std::string, std::string>> get_rows() const;
+    std::vector<std::tuple<bool, std::string, std::string, std::string>> get_rows() const;
     void on_show_subcategories_toggled(bool checked);
     void apply_subcategory_visibility();
     void clear_move_history();
     void record_move_for_undo(int row, const std::string& source, const std::string& destination);
     void handle_selected_row(int row_index,
                              const std::string& file_name,
-                             const std::string& file_type,
                              const std::string& category,
                              const std::string& subcategory,
                              const std::string& base_dir,
diff --git a/app/include/MovableCategorizedFile.hpp b/app/include/MovableCategorizedFile.hpp
index 6a5ff11..86e913b 100644
--- a/app/include/MovableCategorizedFile.hpp
+++ b/app/include/MovableCategorizedFile.hpp
@@ -13,10 +13,9 @@ public:
 
     MovableCategorizedFile();
     MovableCategorizedFile(const std::string& dir_path,
-                    const std::string& cat,
-                    const std::string& subcat,
-                    const std::string& file_name,
-                    const std::string& file_type);
+                           const std::string& cat,
+                           const std::string& subcat,
+                           const std::string& file_name);
     ~MovableCategorizedFile();
     void create_cat_dirs(bool use_subcategory);
     bool move_file(bool use_subcategory);
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/include/CategorizationDialog.hpp`, `app/include/MovableCategorizedFile.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MovableCategorizedFile.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
