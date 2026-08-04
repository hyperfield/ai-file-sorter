# 2025-11-16: chore(readme): update + separate changelog file

## Covered commits
- `dc34b46` `2025-11-16` `chore(readme): update + separate changelog file`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `CHANGELOG.md`
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `dc34b46`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- /dev/null
+++ b/CHANGELOG.md
@@ -0,0 +1,51 @@
+# Changelog
+
+## [1.1.0] - 2025-11-08
+
+- New feature: Support for Vulkan. This means that many non-Nvidia graphics cards (GPUs) are now supported for compute acceleration during local LLM inference.
+- New feature: Toggle subcategories in the categorization review dialog.
+- New feature: Undo the recent file sort (move) action.
+- Fixes: Bug fixes and stability improvements.
+- Added a CTest-integrated test suite. Expanded test coverage.
+- Code optimization refactors.
+
+## [1.0.0] - 2025-10-30
+
+- Migrated the entire desktop UI from GTK/Glade to a native Qt6 interface.
+- Added selection boxes for files in the categorization review dialog.
+- Added internatioinalization framework and the French translation for the user interface.
+- Added refreshed menu icons, mnemonic behaviour, and persistent File Explorer settings.
+- Simplified cross-platform builds (Linux/macOS) around Qt6; retired the MSYS2/GTK toolchain.
+- Optimized and cleaned up the code. Fixed error-prone areas.
+- Modernized the build pipeline. Introduced CMake for compilation on Windows.
+
+## [0.9.7] - 2025-10-19
+
+- Added paths to files in LLM requests for more context.
+- Added taxonomy for more consistent assignment of categories across categorizations.
+  (Narrowing down the number of categories and subcategories).
+- Improved the readability of the categorization progress dialog box.
+- Improved the stability of CUDA detection and interaction.
+- Added more logging coverage throughout the code base.
+
+## [0.9.3] - 2025-09-22
+
+- Added compatibility with CUDA 13.
+
+## [0.9.2] - 2025-08-06
+
+- Bug fixes.
+- Increased code coverage with logging.
+
+## [0.9.1] - 2025-08-01
+
+- Bug fixes.
+- Minor improvements for stability.
+- Removed the deprecated GPU backend from the runtime build.
+
+## [0.9.0] - 2025-07-18
+
+- Local LLM support with `llama.cpp`.
+- LLM selection and download dialog.
+- Improved `Makefile` for a more hassle-free build and installation.
+- Minor bug fixes and improvements.
diff --git a/README.md b/README.md
index a88858c..8924221 100644
--- a/README.md
+++ b/README.md
@@ -47,14 +47,10 @@ File content–based sorting for certain file types is also in development.
 
 - [AI File Sorter](#ai-file-sorter)
   - [Changelog](#changelog)
-    - [[1.1.0] - 2025-11-08](#110---2025-11-08)
-    - [[1.0.0] - 2025-10-30](#100---2025-10-30)
-    - [[0.9.7] - 2025-10-19](#097---2025-10-19)
-    - [[0.9.3] - 2025-09-22](#093---2025-09-22)
-    - [[0.9.2] - 2025-08-06](#092---2025-08-06)
-    - [[0.9.1] - 2025-08-01](#091---2025-08-01)
-    - [[0.9.0] - 2025-07-18](#090---2025-07-18)
   - [Features](#features)
+  - [Categorization](#categorization)
+    - [Categorization modes](#categorization-modes)
+    - [Category whitelists](#category-whitelists)
   - [Requirements](#requirements)
   - [Installation](#installation)
     - [Linux](#linux)
```

The excerpt is taken from the commit diff for `chore(readme): update + separate changelog file`. The most relevant surfaces are `CHANGELOG.md`, `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
