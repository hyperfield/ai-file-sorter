# 2026-01-11: chore(changelog): update

## Covered commits
- `378431a` `2026-01-11` `chore(changelog): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `app/include/external/llama.cpp`, `external/Catch2`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `378431a`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,6 +1,6 @@
 ﻿# Changelog
 
-## [1.5.0] - 2026-01-06
+## [1.5.0] - 2026-01-11
 
 - Added content analysis for picture files via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
 - Added image analysis options in the main window (analyze images, offer rename suggestions, rename-only mode).
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index ae9f8df..ee09828 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
+Subproject commit ee09828cb057460b369576410601a3a09279e23c
```

The excerpt is taken from the commit diff for `chore(changelog): update`. The most relevant surfaces are `CHANGELOG.md`, `app/include/external/llama.cpp`, `external/Catch2`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
