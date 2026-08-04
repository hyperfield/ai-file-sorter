# 2025-11-16: chore(app): refactor to reduce code complexity

## Covered commits
- `6998e2c` `2025-11-16` `chore(app): refactor to reduce code complexity`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/FileScanner.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `tests/unit/test_ui_translator.cpp`

## What changed from what, why, and how
The commit updated test-related files in `app/include/MainApp.hpp`, `app/lib/FileScanner.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `app/lib/Settings.cpp`, `tests/unit/test_ui_translator.cpp`. It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `6998e2c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -120,6 +120,11 @@ private:
     void stop_running_analysis();
     void show_llm_selection_dialog();
     void on_about_activate();
+    void append_progress(const std::string& message);
+    bool should_abort_analysis() const;
+    void prune_empty_cached_entries_for(const std::string& directory_path);
+    void log_cached_highlights();
+    void log_pending_queue();
     void run_consistency_pass();
     void handle_development_prompt_logging(bool checked);
     void record_categorized_metrics(int count);
diff --git a/app/lib/FileScanner.cpp b/app/lib/FileScanner.cpp
index 14b6a39..06ddb6c 100644
--- a/app/lib/FileScanner.cpp
+++ b/app/lib/FileScanner.cpp
@@ -28,6 +28,20 @@ FileScanner::get_directory_entries(const std::string &directory_path,
     const bool include_directories = has_flag(options, FileScanOptions::Directories);
     const bool include_hidden = has_flag(options, FileScanOptions::HiddenFiles);
 
+    const auto resolve_type = [&](const fs::directory_entry& entry,
+                                  const fs::path& entry_path,
+                                  bool is_bundle) -> std::optional<FileType> {
+        const bool is_file = is_bundle || fs::is_regular_file(entry);
+        const bool is_directory = !is_bundle && fs::is_directory(entry);
+        if (include_files && is_file) {
+            return FileType::File;
+        }
+        if (include_directories && is_directory) {
+            return FileType::Directory;
+        }
+        return std::nullopt;
+    };
+
     try {
         const fs::path scan_path = Utils::utf8_to_path(directory_path);
         for (const auto &entry : fs::directory_iterator(scan_path)) {
```

The excerpt is taken from the commit diff for `chore(app): refactor to reduce code complexity`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/lib/FileScanner.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `app/lib/Settings.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
