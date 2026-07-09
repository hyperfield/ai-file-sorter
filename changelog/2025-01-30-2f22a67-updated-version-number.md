# 2025-01-30: Updated version number

## Covered commits
- `2f22a67` `2025-01-30` `Updated version number`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/app_version.hpp`
- `M` `app/startapp.cpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `README.md`, `app/include/app_version.hpp`, `app/startapp.cpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `2f22a67`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,6 +1,6 @@
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/0.8.0/green)](#) [![Donate via PayPal](https://badgen.net/badge/donate/PayPal/blue)](https://paypal.me/aifilesorter)
+[![Version](https://badgen.net/badge/version/0.8.1/green)](#) [![Donate via PayPal](https://badgen.net/badge/donate/PayPal/blue)](https://paypal.me/aifilesorter)
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly.
 
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
index f562c62..fe96819 100644
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{0, 8, 0};
\ No newline at end of file
+const Version APP_VERSION = Version{0, 8, 1};
\ No newline at end of file
```

The excerpt is taken from the commit diff for `Updated version number`. The most relevant surfaces are `README.md`, `app/include/app_version.hpp`, `app/startapp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
