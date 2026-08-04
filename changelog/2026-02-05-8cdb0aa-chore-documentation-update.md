# 2026-02-05: chore(documentation): update

## Covered commits
- `8cdb0aa` `2026-02-05` `chore(documentation): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/include/external/llama.cpp`, `external/Catch2`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `8cdb0aa`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -49,7 +49,7 @@ AI File Sorter runs entirely on your device, using local AI models such as LLaMa
 
 [![Get it from Microsoft](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/detail/9npk4dzd6r6s)
 
-![AI File Sorter Screenshot](images/screenshots/ai-file-sorter-win.gif) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/categorization-dialog-macos.png)
+![AI File Sorter Screenshot](images/screenshots/ai-file-sorter-win.gif) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/sort-confirm-moved-win.png)
 
 ---
 
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index a89002f..ae9f8df 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit a89002f07b55dace8671fc07b2e2418700716992
+Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
```

The excerpt is taken from the commit diff for `chore(documentation): update`. The most relevant surfaces are `README.md`, `app/include/external/llama.cpp`, `external/Catch2`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
