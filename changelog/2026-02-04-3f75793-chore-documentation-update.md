# 2026-02-04: chore(documentation): update

## Covered commits
- `3f75793` `2026-02-04` `chore(documentation): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `3f75793`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,6 +1,6 @@
 ﻿# Changelog
 
-## [1.6.0] - 2026-02-01
+## [1.6.0] - 2026-02-04
 
 - Added document content analysis (text LLM) with rename-only/document-only options and optional creation-date suffixes for categories. Supported document formats include PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
 - Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
@@ -14,7 +14,7 @@
 - Added a system compatibility check (benchmarking) to determine the most suitable LLM for your system.
 - Added Korean as an interface language.
 - macOS builds now include variant `make` targets for Apple Silicon (M1 / M2-M3) and Intel outputs, plus improved arch-aware llama.cpp builds.
-- UI and usability improvements.
+- UI, stability, persistence, and usability improvements.
 
 ## [1.5.0] - 2026-01-11
```

The excerpt is taken from the commit diff for `chore(documentation): update`. The most relevant surfaces are `CHANGELOG.md`, `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
