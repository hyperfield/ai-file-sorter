# 2026-03-08: Update llama.cpp submodule

## Covered commits
- `fbc7c5e` `2026-03-08` `Update llama.cpp submodule`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/include/external/llama.cpp`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `fbc7c5e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
+Subproject commit a95047979a1671be970398a7c8073159ac71013e
```

The excerpt is taken from the commit diff for `Update llama.cpp submodule`. The most relevant surfaces are `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
