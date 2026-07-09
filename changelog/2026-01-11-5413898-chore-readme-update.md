# 2026-01-11: chore(readme): update

## Covered commits
- `5413898` `2026-01-11` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `5413898`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -21,25 +21,26 @@
   <img src="images/platform-logos/logo-linux.png" alt="Linux" width="160">
 </p>
 
-AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.
+AI File Sorter is a cross-platform desktop application that uses AI to organize files and intelligently suggest better file names for image files, based on their visual content. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
 
-It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, taxonomy, and other heuristics for accuracy and consistency.
+The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. All rename suggestions are optional and always require your approval.
 
-The app uses a **taxonomy-based system**, which essentially means that it builds up a smarter internal reference for your file types and naming patterns.
+AI File Sorter helps tidy up cluttered folders such as Downloads, external drives, or NAS storage by automatically grouping files based on their names, extensions, folder context, and learned organization patterns.
 
-The app intelligently assigns categories and optional subcategories, which you can review and adjust before confirming. Once approved, the necessary folders are created and your files are sorted automatically.  
+Instead of relying on fixed rules, the app gradually builds an internal understanding of how your files are typically organized and named. This allows it to make more consistent categorization and naming suggestions over time, while still letting you review and adjust everything before anything is applied.
 
-AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* and *Mistral 7B*, and does not require an internet connection unless you choose to use a remote model.
+Categories (and optional subcategories) are suggested for each file, and for supported file types, rename suggestions are provided as well. Once you confirm, the required folders are created automatically and files are sorted accordingly.
 
-Image content analysis for supported picture files is available; broader file-content sorting is still in development.
+Privacy-first by design:
+AI File Sorter runs entirely on your device, using local AI models such as LLaMa 3B and Mistral 7B. No files, filenames, images, or metadata are uploaded anywhere, and no telemetry is sent. An internet connection is only used if you explicitly choose to enable a remote model.
 
 ---
 
 #### How It Works
 
-1. Point it at a folder or drive  
-2. It runs a local LLM to analyze your files  
-3. The LLM suggests categorizations  
+1. Point the app at a folder or drive  
+2. Files (and image content, when applicable) are analyzed locally  
+3. Category and rename suggestions are generated  
 4. You review and adjust if needed - done  
 
 ---
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
