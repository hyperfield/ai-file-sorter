# 2025-10-29: chore(readme): update

## Covered commits
- `73b5420` `2025-10-29` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `73b5420`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -42,8 +42,10 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - Migrated the entire desktop UI from GTK/Glade to a native Qt6 interface.
 - Replaced GLib/GIO resource usage with Qt resources and modernized build pipeline.
+- Added internatioinalization framework and the French translation for the user interface.
 - Added refreshed menu icons, mnemonic behaviour, and persistent File Explorer settings.
 - Simplified cross-platform builds (Linux/macOS) around Qt6; retired the MSYS2/GTK toolchain.
+- Optimzed and clean up the code.
 
 ### [0.9.7] - 2025-10-19
 
@@ -169,7 +171,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
 6. **Compile the application**
    ```bash
    cd app
-   make
+   make -j4
    sudo make install   # optional
    ```
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
