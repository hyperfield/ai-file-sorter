# 2025-11-03: chore(readme): update

## Covered commits
- `2d4236b` `2025-11-03` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `2d4236b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -3,7 +3,26 @@
 
 [![Version](https://badgen.net/badge/version/1.0.0/green)](#)
 
-AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a modern Qt6 interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice. It uses a taxonomy-based system, so the more files you sort, the more consistent and accurate the categories become over time. It essentially builds up a smarter internal reference for your file types and naming patterns. Also, file content-based sorting for some file types is coming up as well.
+AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.  
+
+It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, and taxonomy.  
+
+It uses a **taxonomy-based system**, so the more files you sort, the more consistent and accurate the categories become over time. It essentially builds up a smarter internal reference for your file types and naming patterns. File content–based sorting for certain file types is also in development.  
+
+The app intelligently assigns categories and optional subcategories, which you can review and adjust before confirming. Once approved, the necessary folders are created and your files are sorted automatically.  
+
+AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* and *Mistral 7B*, and does not require an internet connection unless you choose to use a remote model.
+
+---
+
+#### How It Works
+
+1. Point it at a folder or drive  
+2. It runs a local LLM to analyze your files  
+3. The LLM suggests categorizations  
+4. You review and adjust if needed — done  
+
+---
 
 [![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
