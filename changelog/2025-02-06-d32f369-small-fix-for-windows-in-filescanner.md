# 2025-02-06: Small fix for Windows in FileScanner

## Covered commits
- `d32f369` `2025-02-06` `Small fix for Windows in FileScanner`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/FileScanner.cpp`

## What changed from what, why, and how
The commit modified `app/lib/FileScanner.cpp`. It changed the repository from the prior state to the state described by `Small fix for Windows in FileScanner`.

Before this commit, the repository reflected the state immediately preceding `d32f369`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/FileScanner.cpp b/app/lib/FileScanner.cpp
--- a/app/lib/FileScanner.cpp
+++ b/app/lib/FileScanner.cpp
@@ -39,7 +39,7 @@ FileScanner::get_directory_entries(const std::string &directory_path,
 
 bool FileScanner::is_file_hidden(const fs::path &path) {
     #ifdef _WIN32
-    DWORD attrs = GetFileAttributes(path.c_str());
+    DWORD attrs = GetFileAttributesW(path.c_str());
     return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_HIDDEN);
     #endif
     return path.string().starts_with(".");
```

The excerpt is taken from the commit diff for `Small fix for Windows in FileScanner`. The most relevant surfaces are `app/lib/FileScanner.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
