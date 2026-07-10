# 2026-01-16: feat(llm): switch 3b default to q4 with legacy q8 option

## Covered commits
- `278a162` `2026-01-16` `feat(llm): switch 3b default to q4 with legacy q8 option`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/include/Types.hpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/resources/.env`

## What changed from what, why, and how
The commit added or exposed new functionality in `README.md`, `app/include/LLMSelectionDialog.hpp`, `app/include/Types.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainApp.cpp`, `app/lib/Settings.cpp`, `app/resources/.env`. It changed the project from not having the capability described by `feat(llm): switch 3b default to q4 with legacy q8 option` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `278a162`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -32,7 +32,7 @@ Instead of relying on fixed rules, the app gradually builds an internal understa
 Categories (and optional subcategories) are suggested for each file, and for supported file types, rename suggestions are provided as well. Once you confirm, the required folders are created automatically and files are sorted accordingly.
 
 Privacy-first by design:
-AI File Sorter runs entirely on your device, using local AI models such as LLaMa 3B and Mistral 7B. No files, filenames, images, or metadata are uploaded anywhere, and no telemetry is sent. An internet connection is only used if you explicitly choose to enable a remote model.
+AI File Sorter runs entirely on your device, using local AI models such as LLaMa 3B (Q4) and Mistral 7B. No files, filenames, images, or metadata are uploaded anywhere, and no telemetry is sent. An internet connection is only used if you explicitly choose to enable a remote model.
 
 ---
 
diff --git a/app/include/LLMSelectionDialog.hpp b/app/include/LLMSelectionDialog.hpp
index 0310509..ae683b1 100644
--- a/app/include/LLMSelectionDialog.hpp
+++ b/app/include/LLMSelectionDialog.hpp
@@ -64,6 +64,7 @@ private:
     void setup_ui();
     void connect_signals();
     void update_ui_for_choice();
+    void update_legacy_local_3b_visibility();
     void update_radio_selection();
     void update_custom_choice_ui();
     void update_openai_fields_state();
```

The excerpt is taken from the commit diff for `feat(llm): switch 3b default to q4 with legacy q8 option`. The most relevant surfaces are `README.md`, `app/include/LLMSelectionDialog.hpp`, `app/include/Types.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainApp.cpp`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
