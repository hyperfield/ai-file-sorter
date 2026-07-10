# 2026-01-29: docs(changelog): update

## Covered commits
- `3437d11` `2026-01-29` `docs(changelog): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `3437d11`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -2,6 +2,8 @@
 
 ## [1.6.0] - 2026-01-16
 
+- Added document content analysis (text LLM) with rename-only/document-only options and optional creation-date suffixes for categories.
+- Added support for document formats including PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
 - Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
 - Improved the LLM selection dialog latency.
 - Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
@@ -11,12 +13,16 @@
 - Review dialog is now scrollable on smaller screens so action buttons stay visible.
 - Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
 - Added Korean as an interface language.
+- UI and usability improvements.
 
 ## [1.5.0] - 2026-01-11
 
 - Added content analysis for picture files via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
 - Added image analysis options in the main window (analyze images, offer rename suggestions, rename-only mode).
 - Added an image-only processing toggle to focus runs on supported picture files and disable standard categorization controls.
+- Added document content analysis (text LLM) with rename-only/document-only modes and optional creation-date suffixes for categories.
+- Added support for document formats including PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
+- Document analysis now uses embedded PDFium/libzip/pugixml in bundled builds (no pdftotext/unzip requirement).
 - Review dialog now supports rename-only flows, suggested filename edits, and status labels for Renamed / Renamed & Moved.
 - Track applied picture renames so already-renamed files are not reprocessed; rename-only review hides them while categorization review keeps them visible for folder moves.
 - Added Dutch as a selectable interface language.
```

The excerpt is taken from the commit diff for `docs(changelog): update`. The most relevant surfaces are `CHANGELOG.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
