# 2026-03-08: docs(release): refresh changelog and UI screenshots

## Covered commits
- `1b8e87f` `2026-03-08` `docs(release): refresh changelog and UI screenshots`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `images/screenshots/aifs-progress-dialog-win.png`
- `M` `images/screenshots/aifs-review-dialog-linux.png`
- `M` `images/screenshots/sort-confirm-moved-win.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `images/screenshots/aifs-progress-dialog-win.png`, `images/screenshots/aifs-review-dialog-linux.png`, `images/screenshots/sort-confirm-moved-win.png`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `1b8e87f`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -5,6 +5,7 @@
 - Progress dialog redesigned into a stage-based table view with explicit stages for Image analysis, Document analysis, and Categorization.
 - Added an image analysis option to append image creation dates (if available) to category names.
 - Added optional audio/video metadata-based filename suggestions for supported media files. When enabled, AI File Sorter can use embedded tags (such as ID3, Vorbis comments, and MP4-style metadata) to propose normalized names like `year_artist_album_title.ext` during review.
+- Bug fixes.
 
 ## [1.6.1] - 2026-02-06
 
diff --git a/images/screenshots/aifs-progress-dialog-win.png b/images/screenshots/aifs-progress-dialog-win.png
index 0b9db0f..6f9f336 100644
Binary files a/images/screenshots/aifs-progress-dialog-win.png and b/images/screenshots/aifs-progress-dialog-win.png differ
diff --git a/images/screenshots/aifs-review-dialog-linux.png b/images/screenshots/aifs-review-dialog-linux.png
index 2eb4e58..542b64c 100644
Binary files a/images/screenshots/aifs-review-dialog-linux.png and b/images/screenshots/aifs-review-dialog-linux.png differ
diff --git a/images/screenshots/sort-confirm-moved-win.png b/images/screenshots/sort-confirm-moved-win.png
index 761e153..c5d409c 100644
Binary files a/images/screenshots/sort-confirm-moved-win.png and b/images/screenshots/sort-confirm-moved-win.png differ
```

The excerpt is taken from the commit diff for `docs(release): refresh changelog and UI screenshots`. The most relevant surfaces are `CHANGELOG.md`, `images/screenshots/aifs-progress-dialog-win.png`, `images/screenshots/aifs-review-dialog-linux.png`, `images/screenshots/sort-confirm-moved-win.png`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
