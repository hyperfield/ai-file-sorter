# 2025-08-11: Minor edits

## Covered commits
- `1b0fa7c` `2025-08-11` `Minor edits`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/lib/CategorizationDialog.cpp`

## What changed from what, why, and how
The commit modified `README.md`, `app/lib/CategorizationDialog.cpp`. It changed the repository from the prior state to the state described by `Minor edits`.

Before this commit, the repository reflected the state immediately preceding `1b0fa7c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -175,6 +175,8 @@ pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkm
 
         powershell -ExecutionPolicy Bypass -File .\build_llama_windows.ps1 cuda=off
 
+    `llama.cpp` will be built from source.
+
 9. **Optional** (not needed if you want to use only local LLMs for file sorting). Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here.
 
 10. Go back the `MSYS2 MINGW64` shell (ensure you ran it *as Administrator*, otherwise `make install` won't work). Go to the cloned repo's `app/resources` path (e.g., `/c/Users/username/repos/ai-file-sorter/app/resources`) and run `bash compile-resources.sh`. Go to the `app` directory (`cd ..`).
diff --git a/app/lib/CategorizationDialog.cpp b/app/lib/CategorizationDialog.cpp
index eaeb95a..dd0204f 100644
--- a/app/lib/CategorizationDialog.cpp
+++ b/app/lib/CategorizationDialog.cpp
@@ -305,7 +305,6 @@ void CategorizationDialog::on_confirm_and_sort_button_clicked()
             if (categorizedFile.move_file(show_subcategory_col)) {
                 const gchar *sorted_icon = "emblem-default";
                 gtk_list_store_set(liststore, &iter, 5, sorted_icon, -1);
-                core_logger->info("File {} moved successfully.", file_name);
             } else {
                 const gchar *sorted_icon = "process-stop";
                 gtk_list_store_set(liststore, &iter, 5, sorted_icon, -1);
```

The excerpt is taken from the commit diff for `Minor edits`. The most relevant surfaces are `README.md`, `app/lib/CategorizationDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
