# 2026-02-10: chore(documentation): update readme and screenshots

## Covered commits
- `6e51be8` `2026-02-10` `chore(documentation): update readme and screenshots`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`
- `D` `images/screenshots/before-after/Capture.PNG`
- `A` `images/screenshots/before-after/ai_file_sorter_before_after.png`
- `A` `images/screenshots/before-after/ai_file_sorter_before_after_vertical.png`
- `A` `images/screenshots/before-after/aifs_before_after.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/include/external/llama.cpp`, `external/Catch2`, `images/screenshots/before-after/Capture.PNG`, `images/screenshots/before-after/ai_file_sorter_before_after.png`, `images/screenshots/before-after/ai_file_sorter_before_after_vertical.png`, `images/screenshots/before-after/aifs_before_after.png`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `6e51be8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -23,6 +23,10 @@
 
 AI File Sorter is a cross-platform desktop application that uses AI to organize files and intelligently suggest better file names for both image and document files, based on their visual or textual content. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.
 
+<p align="center">
+  <img src="images/screenshots/before-after/aifs_before_after.png" alt="AI File Sorter before and after organization example">
+</p>
+
 The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. All rename suggestions are optional and always require your approval.
 
 AI File Sorter helps tidy up cluttered folders such as Downloads, external drives, or NAS storage by automatically grouping files based on their names, extensions, folder context, and learned organization patterns.
@@ -128,8 +132,8 @@ See [CHANGELOG.md](CHANGELOG.md) for the full history.
 - **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
 - **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
 - **Sorting Preview**: See how files will be organized before confirming changes.
-- 🧪 **Dry run** / preview-only mode to inspect planned moves without touching files.
-- ↩️ **Persistent Undo** ("Undo last run") even after closing the sort dialog.
+- **Dry run** / preview-only mode to inspect planned moves without touching files.
+- **Persistent Undo** ("Undo last run") even after closing the sort dialog.
 - **Bring your own key**: Paste your OpenAI or Gemini API key once; it's stored locally and reused for remote runs.
 - **Update Notifications**: Get notified about updates - with optional or required update flows.
```

The excerpt is taken from the commit diff for `chore(documentation): update readme and screenshots`. The most relevant surfaces are `README.md`, `app/include/external/llama.cpp`, `external/Catch2`, `images/screenshots/before-after/Capture.PNG`, `images/screenshots/before-after/ai_file_sorter_before_after.png`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
