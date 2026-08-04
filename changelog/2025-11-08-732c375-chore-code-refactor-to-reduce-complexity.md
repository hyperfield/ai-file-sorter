# 2025-11-08: chore(code): refactor to reduce complexity

## Covered commits
- `732c375` `2025-11-08` `chore(code): refactor to reduce complexity`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/lib/FileScanner.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `README.md`, `app/lib/FileScanner.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Utils.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `732c375`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -61,6 +61,10 @@ AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* an
 
 ## Changelog
 
+### [1.1.0] - 2025-11-08
+
+
+
 ### [1.0.0] - 2025-10-30
 
 - Migrated the entire desktop UI from GTK/Glade to a native Qt6 interface.
diff --git a/app/lib/FileScanner.cpp b/app/lib/FileScanner.cpp
index 4713f35..14b6a39 100644
--- a/app/lib/FileScanner.cpp
+++ b/app/lib/FileScanner.cpp
@@ -24,45 +24,42 @@ FileScanner::get_directory_entries(const std::string &directory_path,
         logger->debug("Scanning directory '{}' with options mask {}", directory_path, static_cast<int>(options));
     }
 
+    const bool include_files = has_flag(options, FileScanOptions::Files);
+    const bool include_directories = has_flag(options, FileScanOptions::Directories);
+    const bool include_hidden = has_flag(options, FileScanOptions::HiddenFiles);
+
     try {
         const fs::path scan_path = Utils::utf8_to_path(directory_path);
         for (const auto &entry : fs::directory_iterator(scan_path)) {
             const fs::path& entry_path = entry.path();
             std::string full_path = Utils::path_to_utf8(entry_path);
             std::string file_name = Utils::path_to_utf8(entry_path.filename());
-            bool is_hidden = is_file_hidden(entry_path);
-            bool should_add = false;
-            FileType file_type;
+            const bool is_hidden = is_file_hidden(entry_path);
 
-            if (is_junk_file(file_name)) continue;
-
-            if (is_file_bundle(entry_path)) {
-                if (has_flag(options, FileScanOptions::Files) &&
-                    (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden)) {
-                    file_type = FileType::File;
-                    should_add = true;
-                }
+            if (is_junk_file(file_name)) {
+                continue;
             }
-            else if (fs::is_regular_file(entry)) {
-                if (has_flag(options, FileScanOptions::Files) &&
-                    (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden)) {
-                    file_type = FileType::File;
-                    should_add = true;
-                }
-            }
-            else if (fs::is_directory(entry)) {
-                if (has_flag(options, FileScanOptions::Directories) &&
-                    (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden)) {
-                    file_type = FileType::Directory;
-                    should_add = true;
+            if (is_hidden && !include_hidden) {
+                if (logger) {
+                    logger->trace("Skipping hidden entry '{}'", full_path);
                 }
+                continue;
             }
 
-            if (should_add) {
-                file_paths_and_names.push_back({full_path, file_name, file_type});
-            } else if (logger && is_hidden && !has_flag(options, FileScanOptions::HiddenFiles)) {
-                logger->trace("Skipping hidden entry '{}'", full_path);
+            FileType file_type;
+            const bool bundle = is_file_bundle(entry_path);
+            const bool is_file = bundle || fs::is_regular_file(entry);
+            const bool is_directory = !bundle && fs::is_directory(entry);
+
+            if (include_files && is_file) {
+                file_type = FileType::File;
+            } else if (include_directories && is_directory) {
+                file_type = FileType::Directory;
+            } else {
+                continue;
             }
+
+            file_paths_and_names.push_back({full_path, file_name, file_type});
         }
     } catch (const fs::filesystem_error& ex) {
         if (logger) {
```

The excerpt is taken from the commit diff for `chore(code): refactor to reduce complexity`. The most relevant surfaces are `README.md`, `app/lib/FileScanner.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
