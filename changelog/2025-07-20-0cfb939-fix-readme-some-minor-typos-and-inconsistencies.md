# 2025-07-20: fix(readme): some minor typos and inconsistencies

## Covered commits
- `0cfb939` `2025-07-20` `fix(readme): some minor typos and inconsistencies`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `0cfb939`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -31,14 +31,14 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or a
                                  remote LLM (ChatGPT), depending on your preference.
-- **Offline-Friendly**: Use a local LLM to categorize files entirely e—no internet or API key required.
+- **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
   **Customizable Sorting Rules**: Automatically assign categories and subcategories for granular organization.
 - **Intuitive Interface**: Lightweight and user-friendly for fast, efficient use.
 - **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
 - **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
 - **Sorting Preview**: See how files will be organized before confirming changes.
 - **Secure API Key Encryption**: When using the remote model, your API key is stored securely with encryption.
-- **Update Notifications**: Get notified about updates—with optional or required update flows.
+- **Update Notifications**: Get notified about updates - with optional or required update flows.
 
 ---
 
@@ -52,7 +52,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
   
   Optional:
     - **Git**: For cloning this repository. You can alternatively download the repo in a zip archive.
-    - **OpenAI API Key**: Not needed for local LLMs, but needed to download the models.
+    - **OpenAI API Key**: Not needed for local LLMs.
 
 ---
```

The excerpt is taken from the commit diff for `fix(readme): some minor typos and inconsistencies`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
