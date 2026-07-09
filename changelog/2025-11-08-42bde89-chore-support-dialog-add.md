# 2025-11-08: chore(support-dialog): add

## Covered commits
- `42bde89` `2025-11-08` `chore(support-dialog): add`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.gitignore`
- `M` `app/CMakeLists.txt`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/MainAppTestAccess.hpp`
- `M` `app/include/Settings.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `A` `tests/unit/test_support_prompt.cpp`

## What changed from what, why, and how
The commit updated test-related files in `.gitignore`, `app/CMakeLists.txt`, `app/include/MainApp.hpp`, `app/include/MainAppTestAccess.hpp`, `app/include/Settings.hpp`, `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`, `app/lib/Settings.cpp`, and 1 more file(s). It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `42bde89`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -104,6 +104,7 @@ app/include/llama/*.h
 !app/scripts/
 /tools/
 /reports/
+/logo-workdir-temp/
 
 # Build artifacts
 
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
index 9660802..73fe269 100644
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -315,6 +315,7 @@ if(AI_FILE_SORTER_BUILD_TESTS)
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_local_llm_backend.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_translation.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_categorization_dialog.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_support_prompt.cpp"
     )
 
     target_include_directories(ai_file_sorter_tests PRIVATE
```

The excerpt is taken from the commit diff for `chore(support-dialog): add`. The most relevant surfaces are `.gitignore`, `app/CMakeLists.txt`, `app/include/MainApp.hpp`, `app/include/MainAppTestAccess.hpp`, `app/include/Settings.hpp`, and 4 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
