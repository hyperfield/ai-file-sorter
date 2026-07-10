# 2025-11-10: chore(readme): update

## Covered commits
- `9725b93` `2025-11-10` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `9725b93`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -6,16 +6,18 @@
 <p align="center">
   <img src="app/resources/images/icon_256x256.png" alt="AI File Sorter logo" width="128" height="128">
 </p>
-AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.  
+AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.
 
-It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, and taxonomy.  
+It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, taxonomy, and other heuristics for accuracy and consistency.
 
-It uses a **taxonomy-based system**, so the more files you sort, the more consistent and accurate the categories become over time. It essentially builds up a smarter internal reference for your file types and naming patterns. File content–based sorting for certain file types is also in development.  
+The app uses a **taxonomy-based system**, which essentially means that it builds up a smarter internal reference for your file types and naming patterns.
 
 The app intelligently assigns categories and optional subcategories, which you can review and adjust before confirming. Once approved, the necessary folders are created and your files are sorted automatically.  
 
 AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* and *Mistral 7B*, and does not require an internet connection unless you choose to use a remote model.
 
+File content–based sorting for certain file types is also in development.  
+
 ---
 
 #### How It Works
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
