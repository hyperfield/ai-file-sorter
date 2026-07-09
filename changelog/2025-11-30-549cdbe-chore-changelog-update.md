# 2025-11-30: chore(changelog): update

## Covered commits
- `549cdbe` `2025-11-30` `chore(changelog): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `549cdbe`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,10 +1,11 @@
-# Changelog
+﻿# Changelog
 
 ## [1.4.0] - 2025-12-30
-- Added dry run / preview-only mode with From→To table, no moves performed until you uncheck.
-- Persistent Undo: the latest sort saves a plan file; use Edit → “Undo last run” even after closing dialogs.
+- Added dry run / preview-only mode with From/To table, no moves performed until you uncheck.
+- Persistent Undo: the latest sort saves a plan file; use Edit -> "Undo last run" even after closing dialogs.
 - UI tweaks: Name column auto-resizes, new translations for dry run/undo strings, Undo moved to top of Edit menu.
 - A few more guard rails added.
+- Remote LLM flow now uses your own OpenAI API key (any ChatGPT model supported); the bundled remote key and obfuscation step were removed.
 
 ## [1.3.0] - 2025-11-21
 
@@ -64,3 +65,7 @@
 - LLM selection and download dialog.
 - Improved `Makefile` for a more hassle-free build and installation.
 - Minor bug fixes and improvements.
+
+
+
+
```

The excerpt is taken from the commit diff for `chore(changelog): update`. The most relevant surfaces are `CHANGELOG.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
