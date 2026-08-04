# 2026-02-06: chore(documentation): update

## Covered commits
- `f260b8b` `2026-02-06` `chore(documentation): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `f260b8b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -21,9 +21,9 @@
   <img src="images/platform-logos/logo-linux.png" alt="Linux" width="160">
 </p>
 
-AI File Sorter is a cross-platform desktop application that uses AI to organize files and intelligently suggest better file names for image files, based on their visual content. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
+AI File Sorter is a cross-platform desktop application that uses AI to organize files and intelligently suggest better file names for both image and document files, based on their visual or textual content. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
 
-The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. All rename suggestions are optional and always require your approval.
+The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. All rename suggestions are optional and always require your approval.
 
 AI File Sorter helps tidy up cluttered folders such as Downloads, external drives, or NAS storage by automatically grouping files based on their names, extensions, folder context, and learned organization patterns.
```

The excerpt is taken from the commit diff for `chore(documentation): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
