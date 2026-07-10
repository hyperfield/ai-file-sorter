# 2026-02-01: chore(deps): bump Catch2 and llama.cpp submodules

## Covered commits
- `df7c5a4` `2026-02-01` `chore(deps): bump Catch2 and llama.cpp submodules`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`
- `M` `TESTS.md`
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `CHANGELOG.md`, `README.md`, `TESTS.md`, `app/include/external/llama.cpp`, `external/Catch2`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `df7c5a4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,9 +1,8 @@
 ﻿# Changelog
 
-## [1.6.0] - 2026-01-16
+## [1.6.0] - 2026-02-01
 
-- Added document content analysis (text LLM) with rename-only/document-only options and optional creation-date suffixes for categories.
-- Added support for document formats including PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
+- Added document content analysis (text LLM) with rename-only/document-only options and optional creation-date suffixes for categories. Supported document formats include PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
 - Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
 - Improved the LLM selection dialog latency.
 - Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
diff --git a/README.md b/README.md
index 3ac86c3..c940f24 100644
--- a/README.md
+++ b/README.md
@@ -86,16 +86,19 @@ AI File Sorter runs entirely on your device, using local AI models such as LLaMa
 
 ## Changelog
 
-## [1.5.0] - 2026-01-06
-
-- Added content analysis for picture files via LLaVA.
-- Added picture analysis options in the main window.
-- Review dialog now supports rename-only flows, suggested filename edits, and status labels for Renamed / Renamed & Moved.
-- Added a picture-only processing toggle to focus runs on supported picture files.
-- Review dialog now supports rename-only flows, suggested filename edits, and status labels.
-- Added Dutch as a selectable interface language.
-- Analysis progress dialog output is now localized in all available UI languages.
-- Build and tests updates.
+## [1.6.0] - 2026-02-01
+
+- Added document content analysis (text LLM) with optional creation-date suffixes for categories. Supported document formats include PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
+- Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
+- Improved the LLM selection dialog latency.
+- Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
+- LLM-derived categorizations and rename suggestions are now saved as you go, so progress isn't lost if the app closes unexpectedly.
+- Image analysis now falls back (with a user prompt) to CPU if the GPU has insufficient available memory.
+- Review dialog now lets you select highlighted rows and bulk edit their categories.
+- Review dialog is now scrollable on smaller screens so action buttons stay visible.
+- Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
+- Added Korean as an interface language.
+- UI and usability improvements.
 
 See [CHANGELOG.md](CHANGELOG.md) for the full history.
```

The excerpt is taken from the commit diff for `chore(deps): bump Catch2 and llama.cpp submodules`. The most relevant surfaces are `CHANGELOG.md`, `README.md`, `TESTS.md`, `app/include/external/llama.cpp`, `external/Catch2`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
