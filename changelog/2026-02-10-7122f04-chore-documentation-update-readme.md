# 2026-02-10: chore(documentation): update readme

## Covered commits
- `7122f04` `2026-02-10` `chore(documentation): update readme`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `7122f04`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -24,7 +24,7 @@
 AI File Sorter is a cross-platform desktop application that uses AI to organize files and intelligently suggest better file names for both image and document files, based on their visual or textual content. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
 
 <p align="center">
-  <img src="images/screenshots/before-after/aifs_before_after.png" alt="AI File Sorter before and after organization example">
+  <img src="images/screenshots/before-after/aifs_before_after.png" alt="AI File Sorter before and after organization example" width="400">
 </p>
 
 The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. All rename suggestions are optional and always require your approval.
```

The excerpt is taken from the commit diff for `chore(documentation): update readme`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
