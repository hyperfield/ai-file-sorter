# 2026-02-11: chore(documentation): update screenshots

## Covered commits
- `c1a09fa` `2026-02-11` `chore(documentation): update screenshots`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`
- `A` `images/screenshots/aifs-review-dialog-linux.png`
- `M` `images/screenshots/before-after/aifs_before_after_h.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/include/external/llama.cpp`, `external/Catch2`, `images/screenshots/aifs-review-dialog-linux.png`, `images/screenshots/before-after/aifs_before_after_h.png`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `c1a09fa`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit a89002f07b55dace8671fc07b2e2418700716992
+Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
diff --git a/external/Catch2 b/external/Catch2
index a1faad9..b59f4f3 160000
--- a/external/Catch2
+++ b/external/Catch2
@@ -1 +1 @@
-Subproject commit a1faad9315ece8e7146e9d2263ceb3d42ea0619a
+Subproject commit b59f4f352291a89c4a3fe1488eeccbddeea85625
```

The excerpt is taken from the commit diff for `chore(documentation): update screenshots`. The most relevant surfaces are `app/include/external/llama.cpp`, `external/Catch2`, `images/screenshots/aifs-review-dialog-linux.png`, `images/screenshots/before-after/aifs_before_after_h.png`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
