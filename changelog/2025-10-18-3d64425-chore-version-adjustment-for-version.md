# 2025-10-18: chore(version): adjustment for version

## Covered commits
- `3d64425` `2025-10-18` `chore(version): adjustment for version`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/Version.hpp`
- `M` `app/include/app_version.hpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `README.md`, `app/include/Version.hpp`, `app/include/app_version.hpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `3d64425`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/0.9.3/green)](#)
+[![Version](https://badgen.net/badge/version/0.9.4/green)](#)
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice.
 
@@ -13,6 +13,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - [AI File Sorter](#ai-file-sorter)
   - [Changelog](#changelog)
+    - [[0.9.4] - 2025-10-18](#094---2025-10-18)
     - [[0.9.3] - 2025-09-22](#093---2025-09-22)
     - [[0.9.2] - 2025-08-06](#092---2025-08-06)
     - [[0.9.1] - 2025-08-01](#091---2025-08-01)
```

The excerpt is taken from the commit diff for `chore(version): adjustment for version`. The most relevant surfaces are `README.md`, `app/include/Version.hpp`, `app/include/app_version.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
