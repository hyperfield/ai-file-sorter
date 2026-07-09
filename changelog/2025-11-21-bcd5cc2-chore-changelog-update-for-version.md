# 2025-11-21: chore(changelog): update for version

## Covered commits
- `bcd5cc2` `2025-11-21` `chore(changelog): update for version`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `bcd5cc2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,14 @@
 # Changelog
 
+## [1.3.0] - 2025-11-21
+
+- You can now switch between two categorization modes: More Refined and More Consistent. Choose depending on your folder and use case.
+- Added optional Whitelists - limit the number and names of categories when needed.
+- Added sorting by file names, categories, subcategories in the Categorization Review dialog.
+- You can now add a custom Local LLM in the Select LLM dialog.
+- Multilingual categorization: the file categorization labels can now be assigned in Dutch, French, German, Italian, Polish, Portuguese, Spanish, and Turkish.
+- New interface languages: Dutch, German, Italian, Polish, Portugese, Spanish, and Turkish.
+
 ## [1.1.0] - 2025-11-08
 
 - New feature: Support for Vulkan. This means that many non-Nvidia graphics cards (GPUs) are now supported for compute acceleration during local LLM inference.
```

The excerpt is taken from the commit diff for `chore(changelog): update for version`. The most relevant surfaces are `CHANGELOG.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
