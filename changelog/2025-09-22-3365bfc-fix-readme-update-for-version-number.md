# 2025-09-22: fix(readme): update for version number

## Covered commits
- `3365bfc` `2025-09-22` `fix(readme): update for version number`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/resources/ui/main_window.glade`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`, `app/resources/ui/main_window.glade`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `3365bfc`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/0.9.2/green)](#)
+[![Version](https://badgen.net/badge/version/0.9.3/green)](#)
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice.
 
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index 8e6f8bc..a0374a6 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 8e6f8bc875358968b63e08f7bbbe0a288f29d856
+Subproject commit a0374a67e2924f2e845cdc59dd67d9a44065a89c
```

The excerpt is taken from the commit diff for `fix(readme): update for version number`. The most relevant surfaces are `README.md`, `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`, `app/resources/ui/main_window.glade`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
