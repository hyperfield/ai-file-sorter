# 2025-11-22: fix(app): misc fixes

## Covered commits
- `6756a53` `2025-11-22` `fix(app): misc fixes`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/Settings.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/scripts/build_llama_macos.sh`

## What changed from what, why, and how
The commit corrected behavior in `README.md`, `app/include/Settings.hpp`, `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`, `app/lib/Settings.cpp`, `app/scripts/build_llama_macos.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(app): misc fixes`.

Before this commit, the repository reflected the state immediately preceding `6756a53`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -61,6 +61,7 @@ File content–based sorting for certain file types is also in development.
     - [macOS](#macos)
     - [Windows](#windows)
   - [Uninstallation](#uninstallation)
+  - [Testing](#testing)
   - [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption)
   - [How to Use](#how-to-use)
   - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
@@ -397,6 +398,19 @@ In the same subdirectory `app`, run `sudo make uninstall`.
 
 ---
 
+## Testing
+
+- From the repo root, clean any old cache and run the CTest wrapper:
+  ```bash
+  cd app
+  rm -rf ../build-tests      # clear a cache from another checkout
+  ./scripts/rebuild_and_test.sh
+  ```
+- The script configures to `../build-tests`, builds, then runs `ctest`.
+- If you have multiple copies of the repo (e.g., `ai-file-sorter` and `ai-file-sorter-mac-dist`), each needs its own `build-tests` folder; reusing one from a different path will make CMake complain about mismatched source/build directories.
+
+---
+
 ## How to Use
 
 1. Launch the application (see the last step in [Installation](#installation) according your OS).
```

The excerpt is taken from the commit diff for `fix(app): misc fixes`. The most relevant surfaces are `README.md`, `app/include/Settings.hpp`, `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`, `app/lib/Settings.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
