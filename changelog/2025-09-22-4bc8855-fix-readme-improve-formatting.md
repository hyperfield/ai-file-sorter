# 2025-09-22: fix(readme): improve formatting

## Covered commits
- `4bc8855` `2025-09-22` `fix(readme): improve formatting`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `4bc8855`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -13,9 +13,9 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - [AI File Sorter](#ai-file-sorter)
   - [Changelog](#changelog)
-    - [\[0.9.2\] - 2025-08-06](#092---2025-08-06)
-    - [\[0.9.1\] - 2025-08-01](#091---2025-08-01)
-    - [\[0.9.0\] - 2025-07-18](#090---2025-07-18)
+    - [[0.9.2] - 2025-08-06](#092---2025-08-06)
+    - [[0.9.1] - 2025-08-01](#091---2025-08-01)
+    - [[0.9.0] - 2025-07-18](#090---2025-07-18)
   - [Features](#features)
   - [Requirements](#requirements)
   - [Installation](#installation)
@@ -25,25 +25,25 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
         - [Navigate into the directory](#navigate-into-the-directory)
       - [Compile the app](#compile-the-app)
     - [MacOS](#macos)
-        - [Clone the repository](#clone-the-repository-1)
-        - [Navigate into the directory](#navigate-into-the-directory-1)
+      - [Clone the repository](#clone-the-repository-1)
+      - [Navigate into the directory](#navigate-into-the-directory-1)
       - [Compile the app](#compile-the-app-1)
   - [Uninstallation](#uninstallation)
     - [Linux](#linux)
-        - [Clone the repository](#clone-the-repository-2)
-        - [Navigate into the directory](#navigate-into-the-directory-2)
+      - [Clone the repository](#clone-the-repository-2)
+      - [Navigate into the directory](#navigate-into-the-directory-2)
       - [Compile the app](#compile-the-app-2)
-      - [1. Install the dependencies](#1-install-the-dependencies)
-        - [Debian / Ubuntu](#debian--ubuntu)
-        - [Fedora / RedHat](#fedora--redhat)
-        - [Arch / Manjaro](#arch--manjaro)
-      - [2. Compile `llama.cpp`](#2-compile-llamacpp)
-        - [Debian / Ubuntu](#debian--ubuntu-1)
-        - [Fedora / RedHat](#fedora--redhat-1)
-        - [Arch / Manjaro](#arch--manjaro-1)
-        - [All Linux](#all-linux)
-      - [3. \[Optional\] Get API key](#3-optional-get-api-key)
-      - [4. Compile and install AI File Sorter](#4-compile-and-install-ai-file-sorter)
+        - [1. Install the dependencies](#1-install-the-dependencies)
+          - [Debian / Ubuntu](#debian--ubuntu)
+          - [Fedora / RedHat](#fedora--redhat)
+          - [Arch / Manjaro](#arch--manjaro)
+        - [2. Compile `llama.cpp`](#2-compile-llamacpp)
+          - [Debian / Ubuntu](#debian--ubuntu-1)
+          - [Fedora / RedHat](#fedora--redhat-1)
+          - [Arch / Manjaro](#arch--manjaro-1)
+          - [All Linux](#all-linux)
+        - [3. [Optional] Get API key](#3-optional-get-api-key)
+        - [4. Compile and install AI File Sorter](#4-compile-and-install-ai-file-sorter)
   - [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption)
   - [Uninstallation](#uninstallation-1)
   - [How to Use](#how-to-use)
```

The excerpt is taken from the commit diff for `fix(readme): improve formatting`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
