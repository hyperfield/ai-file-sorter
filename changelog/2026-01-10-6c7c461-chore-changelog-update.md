# 2026-01-10: chore(changelog): update

## Covered commits
- `6c7c461` `2026-01-10` `chore(changelog): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `6c7c461`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -2,10 +2,11 @@
 
 ## [1.5.0] - 2026-01-06
 
-- Added image content analysis via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
+- Added content analysis for picture files via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
 - Added image analysis options in the main window (analyze images, offer rename suggestions, rename-only mode).
 - Added an image-only processing toggle to focus runs on supported picture files and disable standard categorization controls.
 - Review dialog now supports rename-only flows, suggested filename edits, and status labels for Renamed / Renamed & Moved.
+- Track applied picture renames so already-renamed files are not reprocessed; rename-only review hides them while categorization review keeps them visible for folder moves.
 - Build/test updates: mtmd progress callback auto-detection, mtmd-cli build fix, and new Catch2 tests for rename-only caching.
 
 ## [1.4.5] - 2025-12-05
```

The excerpt is taken from the commit diff for `chore(changelog): update`. The most relevant surfaces are `CHANGELOG.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
