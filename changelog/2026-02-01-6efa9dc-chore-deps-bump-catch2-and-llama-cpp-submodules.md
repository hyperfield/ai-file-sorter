# 2026-02-01: chore(deps): bump Catch2 and llama.cpp submodules

## Covered commits
- `6efa9dc` `2026-02-01` `chore(deps): bump Catch2 and llama.cpp submodules`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/include/external/llama.cpp`, `external/Catch2`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `6efa9dc`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit a89002f07b55dace8671fc07b2e2418700716992
+Subproject commit 1aa7c497c5e1929771c4fc3fc8e37c7157fa9bbd
diff --git a/external/Catch2 b/external/Catch2
index a1faad9..b59f4f3 160000
--- a/external/Catch2
+++ b/external/Catch2
@@ -1 +1 @@
-Subproject commit a1faad9315ece8e7146e9d2263ceb3d42ea0619a
+Subproject commit b59f4f352291a89c4a3fe1488eeccbddeea85625
```

The excerpt is taken from the commit diff for `chore(deps): bump Catch2 and llama.cpp submodules`. The most relevant surfaces are `app/include/external/llama.cpp`, `external/Catch2`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
