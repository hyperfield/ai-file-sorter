# 2026-01-10: chore(readme): update

## Covered commits
- `8a38ecb` `2026-01-10` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `8a38ecb`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -82,9 +82,10 @@ Image content analysis for supported picture files is available; broader file-co
 
 ## [1.5.0] - 2026-01-06
 
-- Added image content analysis via LLaVA.
-- Added image analysis options in the main window.
-- Added an image-only processing toggle to focus runs on supported picture files.
+- Added content analysis for picture files via LLaVA.
+- Added picture analysis options in the main window.
+- Review dialog now supports rename-only flows, suggested filename edits, and status labels for Renamed / Renamed & Moved.
+- Added a picture-only processing toggle to focus runs on supported picture files.
 - Review dialog now supports rename-only flows, suggested filename edits, and status labels.
 - Build and tests updates.
 
@@ -465,10 +466,11 @@ What is stored:
 
 - Directory path, file name, and file type (used as a unique key).
 - Category/subcategory, taxonomy id, categorization style, and timestamp.
-- Suggested filename (for image rename suggestions).
+- Suggested filename (for picture rename suggestions).
 - Rename-only flag (used when "Do not categorize picture files (only rename)" is enabled).
+- Rename-applied flag (marks when a rename was executed so it is not offered again).
 
-If you rename or move a file from the Review dialog, the cache entry is updated to the new name. To reset a folder's cache, accept the recategorization prompt or delete the cache file (or point `CATEGORIZATION_CACHE_FILE` to a new filename).
+If you rename or move a file from the Review dialog, the cache entry is updated to the new name. Already-renamed picture files are skipped for visual analysis and rename suggestions on later runs. In the Review dialog, those already-renamed rows are hidden when rename-only is enabled, but they stay visible when categorization is enabled so you can still move them into category folders. To reset a folder's cache, accept the recategorization prompt or delete the cache file (or point `CATEGORIZATION_CACHE_FILE` to a new filename).
 
 ---
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
