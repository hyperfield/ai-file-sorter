# 2025-11-06: chore(version): update

## Covered commits
- `118a684` `2025-11-06` `chore(version): update`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/app_version.hpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `README.md`, `app/include/app_version.hpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `118a684`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/1.0.5/blue)](#)
+[![Version](https://badgen.net/badge/version/1.1.0/blue)](#)
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.  
 
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
index 713fbbb..0f3fe57 100644
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{1, 0, 5};
+const Version APP_VERSION = Version{1, 1, 0};
```

The excerpt is taken from the commit diff for `chore(version): update`. The most relevant surfaces are `README.md`, `app/include/app_version.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
