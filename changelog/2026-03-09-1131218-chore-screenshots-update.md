# 2026-03-09: chore(screenshots): update

## Covered commits
- `1131218` `2026-03-09` `chore(screenshots): update`

## Motivation
This commit refreshed visual documentation so the repository UI captures matched the actual application behavior and appearance. That matters because screenshots are part of release communication, support, and product explanation.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `R100` `images/screenshots/before-after/aifs-llm-download-win.PNG	images/screenshots/aifs-llm-download-win.PNG`
- `D` `images/screenshots/before-after/ai-file-sorter-win-2.gif`
- `D` `images/screenshots/before-after/ai-file-sorter-win.gif`
- `D` `images/screenshots/before-after/ai_file_sorter_before_after.png`
- `D` `images/screenshots/before-after/ai_file_sorter_before_after_vertical.png`
- `D` `images/screenshots/before-after/aifs-before.PNG`
- `D` `images/screenshots/before-after/aifs_before_after.png`
- `M` `images/screenshots/before-after/aifs_before_after_h.png`
- `A` `images/screenshots/before-after/aifs_before_after_v.png`

## What changed from what, why, and how
The commit updated screenshot assets in `README.md`, `images/screenshots/before-after/aifs-llm-download-win.PNG	images/screenshots/aifs-llm-download-win.PNG`, `images/screenshots/before-after/ai-file-sorter-win-2.gif`, `images/screenshots/before-after/ai-file-sorter-win.gif`, `images/screenshots/before-after/ai_file_sorter_before_after.png`, `images/screenshots/before-after/ai_file_sorter_before_after_vertical.png`, `images/screenshots/before-after/aifs-before.PNG`, `images/screenshots/before-after/aifs_before_after.png`, and 2 more file(s). The repository moved from older UI imagery to newer captures that represented the current interface more accurately.

Before this commit, the repository reflected the state immediately preceding `1131218`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -24,7 +24,7 @@
 AI File Sorter is a cross-platform desktop application that uses AI to organize files and suggest cleaner, more consistent names for images, documents, and supported audio/video files. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
 
 <p align="center">
-  <img src="images/screenshots/before-after/aifs_before_after.png" alt="AI File Sorter before and after organization example" width="400">
+  <img src="images/screenshots/before-after/aifs_before_after_v.png" alt="AI File Sorter before and after organization example" width="600">
 </p>
 
 The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. AI File Sorter can also clean up messy audio and video filenames by using the metadata already stored inside supported media files. If tags such as year, artist, album, or title are available, the app can turn them into a clear suggestion like `2024_artist_album_title.mp3`, which you can review, edit, or ignore before any change is applied.
diff --git a/images/screenshots/before-after/aifs-llm-download-win.PNG b/images/screenshots/aifs-llm-download-win.PNG
similarity index 100%
rename from images/screenshots/before-after/aifs-llm-download-win.PNG
rename to images/screenshots/aifs-llm-download-win.PNG
diff --git a/images/screenshots/before-after/ai-file-sorter-win-2.gif b/images/screenshots/before-after/ai-file-sorter-win-2.gif
deleted file mode 100644
index fcc6610..0000000
Binary files a/images/screenshots/before-after/ai-file-sorter-win-2.gif and /dev/null differ
diff --git a/images/screenshots/before-after/ai-file-sorter-win.gif b/images/screenshots/before-after/ai-file-sorter-win.gif
deleted file mode 100644
index db25521..0000000
Binary files a/images/screenshots/before-after/ai-file-sorter-win.gif and /dev/null differ
diff --git a/images/screenshots/before-after/ai_file_sorter_before_after.png b/images/screenshots/before-after/ai_file_sorter_before_after.png
deleted file mode 100644
index d3b0b37..0000000
Binary files a/images/screenshots/before-after/ai_file_sorter_before_after.png and /dev/null differ
diff --git a/images/screenshots/before-after/ai_file_sorter_before_after_vertical.png b/images/screenshots/before-after/ai_file_sorter_before_after_vertical.png
deleted file mode 100644
index 274388f..0000000
Binary files a/images/screenshots/before-after/ai_file_sorter_before_after_vertical.png and /dev/null differ
diff --git a/images/screenshots/before-after/aifs-before.PNG b/images/screenshots/before-after/aifs-before.PNG
deleted file mode 100644
index 6b60055..0000000
Binary files a/images/screenshots/before-after/aifs-before.PNG and /dev/null differ
diff --git a/images/screenshots/before-after/aifs_before_after.png b/images/screenshots/before-after/aifs_before_after.png
deleted file mode 100644
index 9e964d6..0000000
Binary files a/images/screenshots/before-after/aifs_before_after.png and /dev/null differ
diff --git a/images/screenshots/before-after/aifs_before_after_h.png b/images/screenshots/before-after/aifs_before_after_h.png
index c7d0f08..1bce53b 100644
Binary files a/images/screenshots/before-after/aifs_before_after_h.png and b/images/screenshots/before-after/aifs_before_after_h.png differ
diff --git a/images/screenshots/before-after/aifs_before_after_v.png b/images/screenshots/before-after/aifs_before_after_v.png
new file mode 100644
index 0000000..8aa3fed
Binary files /dev/null and b/images/screenshots/before-after/aifs_before_after_v.png differ
```

The excerpt is taken from the commit diff for `chore(screenshots): update`. The most relevant surfaces are `README.md`, `images/screenshots/before-after/aifs-llm-download-win.PNG	images/screenshots/aifs-llm-download-win.PNG`, `images/screenshots/before-after/ai-file-sorter-win-2.gif`, `images/screenshots/before-after/ai-file-sorter-win.gif`, `images/screenshots/before-after/ai_file_sorter_before_after.png`, and 5 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
