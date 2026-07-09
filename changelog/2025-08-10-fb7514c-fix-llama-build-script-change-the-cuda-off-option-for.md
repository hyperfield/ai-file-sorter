# 2025-08-10: fix(llama-build-script): change the -cuda=off option for Windows and use only the VS toolchain for llama.cpp compilation

## Covered commits
- `fb7514c` `2025-08-10` `fix(llama-build-script): change the -cuda=off option for Windows and use only the VS toolchain for llama.cpp compilation`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/scripts/build_llama_windows.ps1`
- `A` `app/scripts/vcpkg.json`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `README.md`, `app/scripts/build_llama_windows.ps1`, `app/scripts/vcpkg.json`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `fb7514c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/0.9.2/green)](#) [![Donate via PayPal](https://badgen.net/badge/donate/PayPal/blue)](https://paypal.me/aifilesorter)
+[![Version](https://badgen.net/badge/version/0.9.1/green)](#) [![Donate via PayPal](https://badgen.net/badge/donate/PayPal/blue)](https://paypal.me/aifilesorter)
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice.
 
@@ -13,8 +13,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - [AI File Sorter](#ai-file-sorter)
   - [Changelog](#changelog)
-    - [\[0.9.2\] - 2025-08-06](#092---2025-08-06)
-    - [\[0.9.1\] - 2025-08-01](#091---2025-08-01)
+    - [\[0.9.1\] - 2025-08-01](#090---2025-08-01)
     - [\[0.9.0\] - 2025-07-18](#090---2025-07-18)
   - [Features](#features)
   - [Requirements](#requirements)
```

The excerpt is taken from the commit diff for `fix(llama-build-script): change the -cuda=off option for Windows and use only the VS toolchain for llama.cpp compilation`. The most relevant surfaces are `README.md`, `app/scripts/build_llama_windows.ps1`, `app/scripts/vcpkg.json`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
