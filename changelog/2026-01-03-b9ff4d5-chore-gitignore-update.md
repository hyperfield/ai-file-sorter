# 2026-01-03: chore(gitignore): update

## Covered commits
- `b9ff4d5` `2026-01-03` `chore(gitignore): update`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.gitignore`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `.gitignore`. It changed the repository support state, metadata, or supporting files in the way described by `chore(gitignore): update`.

Before this commit, the repository reflected the state immediately preceding `b9ff4d5`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -110,6 +110,7 @@ app/include/llama/*.h
 /Testing/
 /changelog/
 /models/
+/rd/
 
 # Build artifacts
```

The excerpt is taken from the commit diff for `chore(gitignore): update`. The most relevant surfaces are `.gitignore`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
