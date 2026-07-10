# 2025-07-22: chore(filescanner): some code style cleanup

## Covered commits
- `f182776` `2025-07-22` `chore(filescanner): some code style cleanup`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/FileScanner.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/FileScanner.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `f182776`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/FileScanner.cpp b/app/lib/FileScanner.cpp
--- a/app/lib/FileScanner.cpp
+++ b/app/lib/FileScanner.cpp
@@ -21,11 +21,13 @@ FileScanner::get_directory_entries(const std::string &directory_path,
 
         bool is_hidden = is_file_hidden(full_path);
 
-        if (has_flag(options, FileScanOptions::Files) && fs::is_regular_file(entry)) {
+        if (has_flag(options, FileScanOptions::Files) &&
+            fs::is_regular_file(entry)) {
             if (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden) {
                 file_type = FileType::File;
             }
-        } else if (has_flag(options, FileScanOptions::Directories) && fs::is_directory(entry)) {
+        } else if (has_flag(options, FileScanOptions::Directories) &&
+            fs::is_directory(entry)) {
             if (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden) {
                 file_type = FileType::Directory;
             }
@@ -40,7 +42,8 @@ FileScanner::get_directory_entries(const std::string &directory_path,
 bool FileScanner::is_file_hidden(const fs::path &path) {
     #ifdef _WIN32
     DWORD attrs = GetFileAttributesW(path.c_str());
-    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_HIDDEN);
+    return (attrs != INVALID_FILE_ATTRIBUTES) &&
+    (attrs & FILE_ATTRIBUTE_HIDDEN);
     #endif
     return path.string().starts_with(".");
-}
\ No newline at end of file
+}
```

The excerpt is taken from the commit diff for `chore(filescanner): some code style cleanup`. The most relevant surfaces are `app/lib/FileScanner.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
