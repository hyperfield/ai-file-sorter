# 2025-11-11: chore(main): refactor for more readable code

## Covered commits
- `f395128` `2025-11-11` `chore(main): refactor for more readable code`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationDialog.hpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/LLMClient.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/startapp_linux.cpp`
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/CategorizationDialog.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LLMClient.cpp`, `app/lib/LocalLLMClient.cpp`, `app/startapp_linux.cpp`, `app/startapp_windows.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `f395128`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -75,6 +75,13 @@ private:
     void apply_subcategory_visibility();
     void clear_move_history();
     void record_move_for_undo(int row, const std::string& source, const std::string& destination);
+    void handle_selected_row(int row_index,
+                             const std::string& file_name,
+                             const std::string& file_type,
+                             const std::string& category,
+                             const std::string& subcategory,
+                             const std::string& base_dir,
+                             std::vector<std::string>& files_not_moved);
     bool undo_move_history();
     void update_status_after_undo();
     bool move_file_back(const std::string& source, const std::string& destination);
diff --git a/app/lib/CategorizationDialog.cpp b/app/lib/CategorizationDialog.cpp
index 8d3f244..03a0084 100644
--- a/app/lib/CategorizationDialog.cpp
+++ b/app/lib/CategorizationDialog.cpp
@@ -303,47 +303,13 @@ void CategorizationDialog::on_confirm_and_sort_button_clicked()
             ++row_index;
             continue;
         }
-        const std::string effective_subcategory = subcategory.empty() ? category : subcategory;
-
-        if (auto& probe = move_probe_slot()) {
-            probe(TestHooks::CategorizationMoveInfo{
-                show_subcategory_column,
-                category,
-                effective_subcategory,
-                file_name
-            });
-            update_status_column(row_index, true);
-            ++row_index;
-            continue;
-        }
-
-        try {
-            MovableCategorizedFile categorized_file(
-                base_dir, category, effective_subcategory,
-                file_name, file_type);
-
-            const auto preview_paths = categorized_file.preview_move_paths(show_subcategory_column);
-
-            categorized_file.create_cat_dirs(show_subcategory_column);
-            bool moved = categorized_file.move_file(show_subcategory_column);
-            update_status_column(row_index, moved);
-
-            if (!moved) {
-                files_not_moved.push_back(file_name);
-                if (core_logger) {
-                    core_logger->warn("File {} already exists in the destination.", file_name);
-                }
-            }
-            if (moved) {
-                record_move_for_undo(row_index, preview_paths.source, preview_paths.destination);
-            }
-        } catch (const std::exception& ex) {
-            update_status_column(row_index, false);
-            files_not_moved.push_back(file_name);
-            if (core_logger) {
-                core_logger->error("Failed to move '{}': {}", file_name, ex.what());
-            }
-        }
+        handle_selected_row(row_index,
+                            file_name,
+                            file_type,
+                            category,
+                            subcategory,
+                            base_dir,
+                            files_not_moved);
         ++row_index;
     }
```

The excerpt is taken from the commit diff for `chore(main): refactor for more readable code`. The most relevant surfaces are `app/include/CategorizationDialog.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LLMClient.cpp`, `app/lib/LocalLLMClient.cpp`, `app/startapp_linux.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
