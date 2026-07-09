# 2026-01-03: chore(readme): update

## Covered commits
- `343ad60` `2026-01-03` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `343ad60`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Code Version](https://img.shields.io/badge/Code-1.4.5-blue)](#)
+[![Code Version](https://img.shields.io/badge/Code-1.5.0-blue)](#)
 [![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dw/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
@@ -76,11 +76,8 @@ File content–based sorting for certain file types is also in development.
 
 ## Changelog
 
-## [1.4.0] - 2025-12-30
-- Added dry run / preview-only mode with From→To table, no moves performed until you uncheck.
-- Persistent Undo: the latest sort saves a plan file; use Edit → “Undo last run” even after closing dialogs.
-- UI tweaks: Name column auto-resizes, new translations for dry run/undo strings, Undo moved to top of Edit menu.
-- A few more guard rails added.
+## [1.5.0] - 2025-12-30
+
 
 See [CHANGELOG.md](CHANGELOG.md) for the full history.
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
