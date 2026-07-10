# 2026-03-09: chore(readme,changelog): update version date

## Covered commits
- `6a7a249` `2026-03-09` `chore(readme,changelog): update version date`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `6a7a249`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,6 +1,6 @@
 ﻿# Changelog
 
-## [1.7.0] - 2026-02-14
+## [1.7.0] - 2026-03-08
 
 - Progress dialog redesigned into a stage-based table view with explicit stages for Image analysis, Document analysis, and Categorization.
 - Added an image analysis option to append image creation dates (if available) to category names.
diff --git a/README.md b/README.md
index 084d770..070093c 100644
--- a/README.md
+++ b/README.md
@@ -93,7 +93,7 @@ AI File Sorter runs entirely on your device, using local AI models such as LLaMa
 
 ## Changelog
 
-## [1.7.0] - 2026-02-14
+## [1.7.0] - 2026-03-08
 
 - Progress dialog redesigned into a stage-based table view with explicit stages for Image analysis, Document analysis, and Categorization.
 - Added an image analysis option to append image creation dates (when available) to category names.
```

The excerpt is taken from the commit diff for `chore(readme,changelog): update version date`. The most relevant surfaces are `CHANGELOG.md`, `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
