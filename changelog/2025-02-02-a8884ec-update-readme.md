# 2025-02-02: Update README

## Covered commits
- `a8884ec` `2025-02-02` `Update README`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `a8884ec`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -8,8 +8,6 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 <a href="https://www.softpedia.com/get/File-managers/AI-File-Sorter.shtml" target="_blank">A review from Softpedia 🗗</a>
 
-<a href="https://github.com/hyperfield/ai-file-sorter" target="_blank">App's website 🗗</a>
-
 ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-1.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-2.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-3.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-4.png)
 
 ---
@@ -46,7 +44,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 ## Requirements
 
-- **Operating System**: Windows, macOS, or Linux with an internet connection  
+- **Operating System**: Windows, macOS, or Linux with a stable internet connection
 - **C++ Compiler**: A recent `g++` version (used in `Makefile`)  
 - **OpenAI API Key**: Required for AI-based categorization  
 - **Dependencies**: Installed during setup (see installation instructions below)
```

The excerpt is taken from the commit diff for `Update README`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
