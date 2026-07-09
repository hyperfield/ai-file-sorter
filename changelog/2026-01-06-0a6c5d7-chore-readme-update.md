# 2026-01-06: chore(readme): update

## Covered commits
- `0a6c5d7` `2026-01-06` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `0a6c5d7`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -80,8 +80,12 @@ Image content analysis for supported image files is available; broader file-cont
 
 ## Changelog
 
-## [1.5.0] - 2025-12-30
+## [1.5.0] - 2026-01-06
 
+- Added image content analysis via LLaVA.
+- Added image analysis options in the main window.
+- Review dialog now supports rename-only flows, suggested filename edits, and status labels.
+- Build and tests updates.
 
 See [CHANGELOG.md](CHANGELOG.md) for the full history.
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
