# 2026-01-10: feat(ui): say picture files in labels

## Covered commits
- `38b0287` `2026-01-10` `feat(ui): say picture files in labels`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/TranslationManager.cpp`
- `M` `app/lib/UiTranslator.cpp`
- `M` `tests/unit/test_ui_translator.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `CHANGELOG.md`, `README.md`, `app/lib/CategorizationDialog.cpp`, `app/lib/TranslationManager.cpp`, `app/lib/UiTranslator.cpp`, `tests/unit/test_ui_translator.cpp`. It changed the project from not having the capability described by `feat(ui): say picture files in labels` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `38b0287`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -4,7 +4,7 @@
 
 - Added image content analysis via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
 - Added image analysis options in the main window (analyze images, offer rename suggestions, rename-only mode).
-- Added an image-only processing toggle to focus runs on supported image files and disable standard categorization controls.
+- Added an image-only processing toggle to focus runs on supported picture files and disable standard categorization controls.
 - Review dialog now supports rename-only flows, suggested filename edits, and status labels for Renamed / Renamed & Moved.
 - Build/test updates: mtmd progress callback auto-detection, mtmd-cli build fix, and new Catch2 tests for rename-only caching.
 
diff --git a/README.md b/README.md
index 4259435..aa69ee5 100644
--- a/README.md
+++ b/README.md
@@ -31,7 +31,7 @@ The app intelligently assigns categories and optional subcategories, which you c
 
 AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* and *Mistral 7B*, and does not require an internet connection unless you choose to use a remote model.
 
-Image content analysis for supported image files is available; broader file-content sorting is still in development.
+Image content analysis for supported picture files is available; broader file-content sorting is still in development.
 
 ---
```

The excerpt is taken from the commit diff for `feat(ui): say picture files in labels`. The most relevant surfaces are `CHANGELOG.md`, `README.md`, `app/lib/CategorizationDialog.cpp`, `app/lib/TranslationManager.cpp`, `app/lib/UiTranslator.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
