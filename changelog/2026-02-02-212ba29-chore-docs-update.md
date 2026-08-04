# 2026-02-02: chore(docs): update

## Covered commits
- `212ba29` `2026-02-02` `chore(docs): update`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`
- `M` `TESTS.md`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/Settings.hpp`
- `M` `app/include/UiTranslator.hpp`
- `M` `app/include/Utils.hpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MainAppUiBuilder.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/UiTranslator.cpp`
- `M` `app/lib/Utils.cpp`
- `M` `tests/unit/test_settings_image_options.cpp`
- `M` `tests/unit/test_ui_translator.cpp`

## What changed from what, why, and how
The commit updated test-related files in `CHANGELOG.md`, `README.md`, `TESTS.md`, `app/include/MainApp.hpp`, `app/include/Settings.hpp`, `app/include/UiTranslator.hpp`, `app/include/Utils.hpp`, `app/lib/MainApp.cpp`, and 6 more file(s). It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `212ba29`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -6,11 +6,12 @@
 - Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
 - Improved the LLM selection dialog latency.
 - Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
-- Image rename suggestions are now saved as you go, so progress isn't lost if the app closes unexpectedly.
+- LLM-derived categorizations and rename suggestions are now saved as you go, so progress isn't lost if the app closes unexpectedly.
 - Image analysis now falls back (with a user prompt) to CPU if the GPU has insufficient available memory.
 - Review dialog now lets you select highlighted rows and bulk edit their categories.
 - Review dialog is now scrollable on smaller screens so action buttons stay visible.
 - Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
+- Added a system compatibility check (benchmarking) to determine the most suitable LLM for your system.
 - Added Korean as an interface language.
 - UI and usability improvements.
 
diff --git a/README.md b/README.md
index 73ccc55..3746c42 100644
--- a/README.md
+++ b/README.md
@@ -98,6 +98,7 @@ AI File Sorter runs entirely on your device, using local AI models such as LLaMa
 - Review dialog now lets you select highlighted rows and bulk edit their categories.
 - Review dialog is now scrollable on smaller screens so action buttons stay visible.
 - Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
+- Added a system compatibility check (benchmarking) to determine the most suitable LLM for your system.
 - Added Korean as an interface language.
 - UI and usability improvements.
```

The excerpt is taken from the commit diff for `chore(docs): update`. The most relevant surfaces are `CHANGELOG.md`, `README.md`, `TESTS.md`, `app/include/MainApp.hpp`, `app/include/Settings.hpp`, and 9 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
