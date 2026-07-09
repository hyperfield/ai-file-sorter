# 2026-03-01: docs(media): document audio and video tag-based rename support

## Covered commits
- `069027b` `2026-03-01` `docs(media): document audio and video tag-based rename support`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`, `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `069027b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -4,6 +4,7 @@
 
 - Progress dialog redesigned into a stage-based table view with explicit stages for Image analysis, Document analysis, and Categorization.
 - Added an image analysis option to append image creation dates (if available) to category names.
+- Added optional audio/video metadata-based filename suggestions for supported media files. When enabled, AI File Sorter can use embedded tags (such as ID3, Vorbis comments, and MP4-style metadata) to propose normalized names like `year_artist_album_title.ext` during review.
 
 ## [1.6.1] - 2026-02-06
 
diff --git a/README.md b/README.md
index 9b01f84..d5ec93c 100644
--- a/README.md
+++ b/README.md
@@ -21,13 +21,13 @@
   <img src="images/platform-logos/logo-linux.png" alt="Linux" width="160">
 </p>
 
-AI File Sorter is a cross-platform desktop application that uses AI to organize files and intelligently suggest better file names for both image and document files, based on their visual or textual content. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
+AI File Sorter is a cross-platform desktop application that uses AI to organize files and suggest cleaner, more consistent names for images, documents, and supported audio/video files. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
 
 <p align="center">
   <img src="images/screenshots/before-after/aifs_before_after.png" alt="AI File Sorter before and after organization example" width="400">
 </p>
 
-The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. All rename suggestions are optional and always require your approval.
+The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. AI File Sorter can also clean up messy audio and video filenames by using the metadata already stored inside supported media files. If tags such as year, artist, album, or title are available, the app can turn them into a clear suggestion like `2024_artist_album_title.mp3`, which you can review, edit, or ignore before any change is applied.
 
 AI File Sorter helps tidy up cluttered folders such as Downloads, external drives, or NAS storage by automatically grouping files based on their names, extensions, folder context, and learned organization patterns.
```

The excerpt is taken from the commit diff for `docs(media): document audio and video tag-based rename support`. The most relevant surfaces are `CHANGELOG.md`, `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
