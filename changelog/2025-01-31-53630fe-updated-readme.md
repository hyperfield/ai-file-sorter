# 2025-01-31: Updated README

## Covered commits
- `53630fe` `2025-01-31` `Updated README`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/Logger.cpp`
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/include/MainApp.hpp`, `app/lib/Logger.cpp`, `app/lib/MainApp.cpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `53630fe`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -4,6 +4,10 @@
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly.
 
+[![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
+
+[A review from Softpedia](https://www.softpedia.com/get/File-managers/AI-File-Sorter.shtml)
+
 ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-1.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-2.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-3.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-4.png)
 
 ---
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
index 054919f..1b3d431 100644
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -68,6 +68,7 @@ private:
     GtkCheckButton *use_subcategories_checkbox;
     GtkCheckButton *categorize_files_checkbox;
     GtkCheckButton *categorize_directories_checkbox;
+    std::shared_ptr<spdlog::logger> core_logger;
     std::shared_ptr<spdlog::logger> ui_logger;
 
     GtkApplication *create_app();
```

The excerpt is taken from the commit diff for `Updated README`. The most relevant surfaces are `README.md`, `app/include/MainApp.hpp`, `app/lib/Logger.cpp`, `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
