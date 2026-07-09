# 2026-01-21: chore(changelog): update

## Covered commits
- `ef8935e` `2026-01-21` `chore(changelog): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `CHANGELOG.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `ef8935e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -2,11 +2,12 @@
 
 ## [1.6.0] - 2026-01-16
 
-- Local 3B model download now defaults to Q4 for better GPU compatibility.
-- Legacy Local 3B Q8 is still selectable when an existing download is found.
-- LLM selection dialog now uses local file sizes for completed downloads when remote size metadata is unavailable.
-- Added custom OpenAI-compatible API endpoints (base URL + model + optional key) to the Select LLM dialog.
-- Bug fixes
+- Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
+- Improved the LLM selection dialog latency.
+- Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
+- Image rename suggestions are now saved as you go, so progress isn't lost if the app closes unexpectedly.
+- Image analysis now falls back (with a user prompt) to CPU if the GPU has insufficient available memory.
+- Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
 
 ## [1.5.0] - 2026-01-11
```

The excerpt is taken from the commit diff for `chore(changelog): update`. The most relevant surfaces are `CHANGELOG.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
