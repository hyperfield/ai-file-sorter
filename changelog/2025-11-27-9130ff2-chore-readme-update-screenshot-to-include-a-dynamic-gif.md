# 2025-11-27: chore(readme): update screenshot to include a dynamic gif

## Covered commits
- `9130ff2` `2025-11-27` `chore(readme): update screenshot to include a dynamic gif`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `A` `images/screenshots/ai-file-sorter-win.gif`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `images/screenshots/ai-file-sorter-win.gif`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `9130ff2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -45,7 +45,7 @@ File content–based sorting for certain file types is also in development.
 
 [![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 
-![AI File Sorter Screenshot](images/screenshots/aifs-main-window-win.png) ![AI File Sorter Screenshot](images/screenshots/aifs-progress-dialog-win.png) ![AI File Sorter Screenshot](images/screenshots/aifs-review-dialog-win.png) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/categorization-dialog-macos.png)
+![AI File Sorter Screenshot](images/screenshots/ai-file-sorter-win.gif) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/categorization-dialog-macos.png)
 
 ---
 
diff --git a/images/screenshots/ai-file-sorter-win.gif b/images/screenshots/ai-file-sorter-win.gif
new file mode 100644
index 0000000..18da9f5
Binary files /dev/null and b/images/screenshots/ai-file-sorter-win.gif differ
```

The excerpt is taken from the commit diff for `chore(readme): update screenshot to include a dynamic gif`. The most relevant surfaces are `README.md`, `images/screenshots/ai-file-sorter-win.gif`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
