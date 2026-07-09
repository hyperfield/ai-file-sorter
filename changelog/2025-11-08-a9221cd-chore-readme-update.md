# 2025-11-08: chore(readme): update

## Covered commits
- `a9221cd` `2025-11-08` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `a9221cd`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -115,9 +115,9 @@ AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* an
 
 ## Features
 
-- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or a
-                                 remote LLM (ChatGPT), depending on your preference.
+- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or a remote LLM (ChatGPT), depending on your preference.
 - **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
+  **Robust Categorization Algorithm**: Consistency across categories is supported by taxonomy and heuristics.
   **Customizable Sorting Rules**: Automatically assign categories and subcategories for granular organization.
 - **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
 - **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
