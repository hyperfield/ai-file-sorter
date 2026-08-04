# 2026-01-24: chore(readme): update

## Covered commits
- `faed5cd` `2026-01-24` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `faed5cd`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -111,7 +111,7 @@ See [CHANGELOG.md](CHANGELOG.md) for the full history.
 - **Image content analysis (Visual LLM)**: Analyze supported picture files with LLaVA to produce descriptions and optional filename suggestions (rename-only mode supported).
 - **Sortable review**: Sort the Categorization Review table by file name, category, or subcategory to triage faster.
 - **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
-- **Interface languages**: English, Dutch, French, German, Italian, Spanish, and Turkish.
+- **Interface languages**: English, Dutch, French, German, Italian, Korean, Spanish, and Turkish.
 - **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
 - **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
 - **Sorting Preview**: See how files will be organized before confirming changes.
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
