# 2026-02-02: chore(submodule): update llama.cpp

## Covered commits
- `3fd259f` `2026-02-02` `chore(submodule): update llama.cpp`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/include/external/llama.cpp`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `3fd259f`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 1aa7c497c5e1929771c4fc3fc8e37c7157fa9bbd
+Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
```

The excerpt is taken from the commit diff for `chore(submodule): update llama.cpp`. The most relevant surfaces are `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
