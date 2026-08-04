# 2026-02-06: docs(update): subversion update

## Covered commits
- `f29262a` `2026-02-06` `docs(update): subversion update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `f29262a`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,9 @@
 ﻿# Changelog
 
+## [1.6.1] - 2026-02-06
+
+- Local text LLM now prompts to switch to CPU when GPU initialization or inference fails.
+
 ## [1.6.0] - 2026-02-04
 
 - Added document content analysis (text LLM) with rename-only/document-only options and optional creation-date suffixes for categories. Supported document formats include PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
diff --git a/README.md b/README.md
index 3d53211..96b021a 100644
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Code Version](https://img.shields.io/badge/Code-1.6.0-blue)](#)
+[![Code Version](https://img.shields.io/badge/Code-1.6.1-blue)](#)
 [![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dw/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
```

The excerpt is taken from the commit diff for `docs(update): subversion update`. The most relevant surfaces are `CHANGELOG.md`, `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
