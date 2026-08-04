# 2025-11-15: chore(readme): update

## Covered commits
- `3044d8e` `2025-11-15` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.codacy/codacy.yaml`
- `M` `README.md`
- `A` `images/platform-logos/logo-cuda.png`
- `A` `images/platform-logos/logo-linux.png`
- `A` `images/platform-logos/logo-macos.png`
- `A` `images/platform-logos/logo-vulkan.png`
- `A` `images/platform-logos/logo-windows.png`
- `R100` `screenshots/aifs-main-window-win.png	images/screenshots/aifs-main-window-win.png`
- `R100` `screenshots/aifs-progress-dialog-win.png	images/screenshots/aifs-progress-dialog-win.png`
- `R100` `screenshots/aifs-review-dialog-win.png	images/screenshots/aifs-review-dialog-win.png`
- `R100` `screenshots/categorization-dialog-macos.png	images/screenshots/categorization-dialog-macos.png`
- `R100` `screenshots/main_windows_macos.png	images/screenshots/main_windows_macos.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `.codacy/codacy.yaml`, `README.md`, `images/platform-logos/logo-cuda.png`, `images/platform-logos/logo-linux.png`, `images/platform-logos/logo-macos.png`, `images/platform-logos/logo-vulkan.png`, `images/platform-logos/logo-windows.png`, `screenshots/aifs-main-window-win.png	images/screenshots/aifs-main-window-win.png`, and 4 more file(s). It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `3044d8e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.codacy/codacy.yaml b/.codacy/codacy.yaml
--- a/.codacy/codacy.yaml
+++ b/.codacy/codacy.yaml
@@ -1,15 +1,9 @@
 runtimes:
-    - dart@3.7.2
-    - go@1.22.3
     - java@17.0.10
-    - node@22.2.0
     - python@3.11.11
 tools:
-    - dartanalyzer@3.7.2
-    - eslint@8.57.0
     - lizard@1.17.31
-    - pmd@7.11.0
-    - pylint@3.3.6
-    - revive@1.7.0
+    - pmd@6.55.0
+    - pylint@3.3.9
     - semgrep@1.78.0
     - trivy@0.66.0
diff --git a/README.md b/README.md
index e4029ba..d8dcd91 100644
--- a/README.md
+++ b/README.md
@@ -1,11 +1,20 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/1.1.0/blue)](#)
+[![Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter)](#)
 
 <p align="center">
   <img src="app/resources/images/icon_256x256.png" alt="AI File Sorter logo" width="128" height="128">
 </p>
+
+<p align="center">
+  <img src="images/platform-logos/logo-vulkan.png" alt="Vulkan" width="160">
+  <img src="images/platform-logos/logo-cuda.png" alt="CUDA" width="160">
+  <img src="images/platform-logos/logo-windows.png" alt="Windows" width="160">
+  <img src="images/platform-logos/logo-macos.png" alt="macOS" width="160">
+  <img src="images/platform-logos/logo-linux.png" alt="Linux" width="160">
+</p>
+
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.
 
 It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, taxonomy, and other heuristics for accuracy and consistency.
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `.codacy/codacy.yaml`, `README.md`, `images/platform-logos/logo-cuda.png`, `images/platform-logos/logo-linux.png`, `images/platform-logos/logo-macos.png`, and 7 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
