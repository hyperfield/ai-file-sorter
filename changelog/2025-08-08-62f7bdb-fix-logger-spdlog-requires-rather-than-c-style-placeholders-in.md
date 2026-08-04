# 2025-08-08: fix(logger): spdlog requires {} rather than C-style placeholders in strings

## Covered commits
- `62f7bdb` `2025-08-08` `fix(logger): spdlog requires {} rather than C-style placeholders in strings`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/MainApp.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(logger): spdlog requires {} rather than C-style placeholders in strings`.

Before this commit, the repository reflected the state immediately preceding `62f7bdb`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -373,15 +373,15 @@ void MainApp::perform_analysis()
 std::vector<FileEntry>
 MainApp::get_actual_files(const std::string& directory_path)
 {
-    core_logger->info("Getting actual files from directory %s", directory_path.c_str());
+    core_logger->info("Getting actual files from directory {}", directory_path);
 
     std::vector<FileEntry> actual_files =
         dirscanner.get_directory_entries(directory_path, FileScanOptions::Files | FileScanOptions::Directories);
     
-    core_logger->info("Actual files found: %d\n", static_cast<int>(actual_files.size()));
+    core_logger->info("Actual files found: {}", static_cast<int>(actual_files.size()));
 
     for (const auto& [full_file_path, file_name, file_type] : actual_files) {
-        core_logger->info("File: %s, Path: %s\n", file_name.c_str(), full_file_path.c_str());
+        core_logger->info("File: {}, Path: {}", file_name, full_file_path);
     }
 
     return actual_files;
@@ -428,7 +428,7 @@ void MainApp::on_analyze_button_clicked(GtkButton *button, gpointer main_app_ins
         try {
             app->perform_analysis();
         } catch (const std::exception &ex) {
-            app->core_logger->error("Exception during analysis: %s\n", ex.what());
+            app->core_logger->error("Exception during analysis: {}", ex.what());
         }
     });
 }
```

The excerpt is taken from the commit diff for `fix(logger): spdlog requires {} rather than C-style placeholders in strings`. The most relevant surfaces are `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
