# 2025-11-22: chore(codacy): code style definition

## Covered commits
- `aa256db` `2025-11-22` `chore(codacy): code style definition`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `.clang-format`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `.clang-format`. It changed the repository support state, metadata, or supporting files in the way described by `chore(codacy): code style definition`.

Before this commit, the repository reflected the state immediately preceding `aa256db`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.clang-format b/.clang-format
--- /dev/null
+++ b/.clang-format
@@ -0,0 +1,13 @@
+BasedOnStyle: Google
+IndentWidth: 4
+TabWidth: 4
+UseTab: Never
+ColumnLimit: 120
+BreakBeforeBraces: Attach
+AllowShortFunctionsOnASingleLine: Inline
+PointerAlignment: Left
+DerivePointerAlignment: false
+SpacesInParentheses: false
+SpaceAfterCStyleCast: true
+SpaceBeforeParens: ControlStatements
+SortIncludes: true
```

The excerpt is taken from the commit diff for `chore(codacy): code style definition`. The most relevant surfaces are `.clang-format`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
