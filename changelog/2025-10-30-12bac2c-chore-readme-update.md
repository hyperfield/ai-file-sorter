# 2025-10-30: chore(readme): update

## Covered commits
- `12bac2c` `2025-10-30` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/external/llama.cpp`
- `D` `screenshots/AI-File-Sorter_screenshot-1.png`
- `D` `screenshots/AI-File-Sorter_screenshot-2.png`
- `D` `screenshots/AI-File-Sorter_screenshot-3.png`
- `D` `screenshots/AI-File-Sorter_screenshot-4.png`
- `A` `screenshots/aifs-main-window-win.png`
- `A` `screenshots/aifs-progress-dialog-win.png`
- `A` `screenshots/aifs-review-dialog-win.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/include/external/llama.cpp`, `screenshots/AI-File-Sorter_screenshot-1.png`, `screenshots/AI-File-Sorter_screenshot-2.png`, `screenshots/AI-File-Sorter_screenshot-3.png`, `screenshots/AI-File-Sorter_screenshot-4.png`, `screenshots/aifs-main-window-win.png`, `screenshots/aifs-progress-dialog-win.png`, and 1 more file(s). It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `12bac2c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -7,13 +7,13 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 [![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 
-![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-1.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-2.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-3.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-4.png) ![AI File Sorter Screenshot](screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](screenshots/categorization-dialog-macos.png)
+![AI File Sorter Screenshot](screenshots/aifs-main-window-win.png) ![AI File Sorter Screenshot](screenshots/aifs-progress-dialog-win.png) ![AI File Sorter Screenshot](screenshots/aifs-review-dialog-win.png) ![AI File Sorter Screenshot](screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](screenshots/categorization-dialog-macos.png)
 
 ---
 
 - [AI File Sorter](#ai-file-sorter)
   - [Changelog](#changelog)
-    - [[1.0.0] - 2026-02-18](#100---2026-02-18)
+    - [[1.0.0] - 2026-02-18](#100---2025-10-30)
     - [[0.9.7] - 2025-10-19](#097---2025-10-19)
     - [[0.9.3] - 2025-09-22](#093---2025-09-22)
     - [[0.9.2] - 2025-08-06](#092---2025-08-06)
@@ -38,7 +38,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 ## Changelog
 
-### [1.0.0] - 2026-02-18
+### [1.0.0] - 2025-10-30
 
 - Migrated the entire desktop UI from GTK/Glade to a native Qt6 interface.
 - Added selection boxes for files in the categorization review dialog.
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`, `app/include/external/llama.cpp`, `screenshots/AI-File-Sorter_screenshot-1.png`, `screenshots/AI-File-Sorter_screenshot-2.png`, `screenshots/AI-File-Sorter_screenshot-3.png`, and 4 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
