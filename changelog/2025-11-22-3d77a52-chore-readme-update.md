# 2025-11-22: chore(readme): update

## Covered commits
- `3d77a52` `2025-11-22` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `3d77a52`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -2,6 +2,10 @@
 # AI File Sorter
 
 [![Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter)](#)
+[![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
+[![SourceForge Downloads](https://img.shields.io/sourceforge/dw/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
+[![Codacy Badge](https://app.codacy.com/project/badge/Grade/2c646c836a9844be964fbf681649c3cd)](https://app.codacy.com/gh/hyperfield/ai-file-sorter/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
+[![Donate](https://img.shields.io/badge/Support%20AI%20File%20Sorter-orange)](https://filesorter.app/donate)
 
 <p align="center">
   <img src="app/resources/images/icon_256x256.png" alt="AI File Sorter logo" width="128" height="128">
@@ -79,13 +83,17 @@ See [CHANGELOG.md](CHANGELOG.md) for the release history.
 - **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
   **Robust Categorization Algorithm**: Consistency across categories is supported by taxonomy and heuristics.
   **Customizable Sorting Rules**: Automatically assign categories and subcategories for granular organization.
+- **Two categorization modes**: Pick **More Refined** for detailed labels or **More Consistent** to bias toward uniform categories within a folder.
+- **Category whitelists**: Define named whitelists of allowed categories/subcategories, manage them under **Settings → Manage category whitelists…**, and toggle/select them in the main window when you want to constrain model output for a session.
+- **Multilingual categorization**: Have the LLM assign categories in Dutch, French, German, Italian, Polish, Portuguese, Spanish, or Turkish (model dependent).
+- **Custom local LLMs**: Register your own local GGUF models directly from the **Select LLM** dialog.
+- **Sortable review**: Sort the Categorization Review table by file name, category, or subcategory to triage faster.
 - **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
 - **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
 - **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
 - **Sorting Preview**: See how files will be organized before confirming changes.
 - **Secure API Key Encryption**: When using the remote model, your API key is stored securely with encryption.
 - **Update Notifications**: Get notified about updates - with optional or required update flows.
-- **Category Whitelists**: Define named whitelists of allowed categories/subcategories, manage them under **Settings → Manage category whitelists…**, and toggle/select them in the main window via **Use a whitelist** when you want to constrain model output for a session.
 
 ---
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
