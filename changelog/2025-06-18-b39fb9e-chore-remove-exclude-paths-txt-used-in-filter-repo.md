# 2025-06-18: chore: remove exclude-paths.txt (used in filter-repo)

## Covered commits
- `b39fb9e` `2025-06-18` `chore: remove exclude-paths.txt (used in filter-repo)`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `D` `exclude-paths.txt`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `exclude-paths.txt`. It changed the repository support state, metadata, or supporting files in the way described by `chore: remove exclude-paths.txt (used in filter-repo)`.

Before this commit, the repository reflected the state immediately preceding `b39fb9e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/exclude-paths.txt b/exclude-paths.txt
--- a/exclude-paths.txt
+++ /dev/null
@@ -1,4 +0,0 @@
-app/test_local_llm
-app/test_local_llm_chat
-app/test_local_llm_chat.cpp
-app/test_local_llm.cpp
```

The excerpt is taken from the commit diff for `chore: remove exclude-paths.txt (used in filter-repo)`. The most relevant surfaces are `exclude-paths.txt`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
