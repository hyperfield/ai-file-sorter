# 2026-02-25: chore(version): update

## Covered commits
- `0a2ac16` `2026-02-25` `chore(version): update`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`
- `M` `app/include/app_version.hpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `CHANGELOG.md`, `README.md`, `app/include/app_version.hpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `0a2ac16`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,10 @@
 ﻿# Changelog
 
+## [1.6.2] - 2026-02-25
+
+- Fixed category parsing so non-standard LLM output formats no longer create malformed merged folder names.
+- Expanded taxonomy normalization to collapse common category synonyms (for example backups/archives, images/media/photos, documents/texts/papers, software/installers/updates).
+
 ## [1.6.1] - 2026-02-06
 
 - Local text LLM now prompts to switch to CPU when GPU initialization or inference fails.
diff --git a/README.md b/README.md
index ef65412..be542d5 100644
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Code Version](https://img.shields.io/badge/Code-1.6.1-blue)](#)
+[![Code Version](https://img.shields.io/badge/Code-1.6.2-blue)](#)
 [![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dw/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
```

The excerpt is taken from the commit diff for `chore(version): update`. The most relevant surfaces are `CHANGELOG.md`, `README.md`, `app/include/app_version.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
