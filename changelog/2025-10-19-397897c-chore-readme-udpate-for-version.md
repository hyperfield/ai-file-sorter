# 2025-10-19: chore(readme): udpate for version

## Covered commits
- `397897c` `2025-10-19` `chore(readme): udpate for version`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `397897c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -59,8 +59,11 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 ## Changelog
 
-### [0.9.4] - 2025-10-19
+### [0.9.7] - 2025-10-19
 
+- Added paths to files in LLM requests for more context.
+- Added taxonomy for more consistent assignment of categories across categorizations.
+  (Narrowing down the number of categories and subcategories).
 - Added more logging coverage throughout the code base.
 
 ### [0.9.3] - 2025-09-22
```

The excerpt is taken from the commit diff for `chore(readme): udpate for version`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
