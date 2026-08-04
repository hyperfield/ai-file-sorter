# 2026-03-21: chore(documents):  update

## Covered commits
- `cd7684c` `2026-03-21` `chore(documents):  update`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `CHANGELOG.md`, `README.md`. It changed the repository support state, metadata, or supporting files in the way described by `chore(documents):  update`.

Before this commit, the repository reflected the state immediately preceding `cd7684c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,14 @@
 ﻿# Changelog
 
+## [1.7.1] - 2026-03-21
+
+- Local categorization with local LLMs is now more robust: prompt budgeting, output sanitization, and category/subcategory parsing were hardened so verbose or oddly formatted replies no longer cause widespread invalid categorization failures.
+- Recursive scans now tolerate unreadable subfolders and other filesystem errors instead of aborting the overall run.
+- Cached category labels are sanitized more aggressively to avoid malformed UTF-8 data breaking later categorization or display.
+- macOS local-LLM packaging/runtime handling was hardened: bundled llama/ggml dylibs are now relocatable, and the app no longer falls back to conflicting system/Homebrew ggml libraries during backend loading.
+- Linux/macOS build and packaging flows were improved, including staged PDFium runtime files, better Debian package dependencies, CPU/CUDA/Vulkan Debian package variants, and improved Homebrew MediaInfo detection on macOS source builds.
+- Added cross-platform diagnostics collection scripts for Linux, macOS, and Windows.
+
 ## [1.7.0] - 2026-03-08
 
 - Progress dialog redesigned into a stage-based table view with explicit stages for Image analysis, Document analysis, and Categorization.
diff --git a/README.md b/README.md
index 67f47d6..f38fba6 100644
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Code Version](https://img.shields.io/badge/Code-1.7.0-blue)](#)
+[![Code Version](https://img.shields.io/badge/Code-1.7.1-blue)](#)
 [![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
 ![filesorter.app Downloads](https://filesorter.app/download-stats/badge.svg)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
```

The excerpt is taken from the commit diff for `chore(documents):  update`. The most relevant surfaces are `CHANGELOG.md`, `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
