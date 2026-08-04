# 2026-02-04: chore(screenshots): update

## Covered commits
- `b88eb67` `2026-02-04` `chore(screenshots): update`

## Motivation
This commit refreshed visual documentation so the repository UI captures matched the actual application behavior and appearance. That matters because screenshots are part of release communication, support, and product explanation.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `external/Catch2`
- `A` `images/screenshots/aifs-benchmark-dialog-win.png`
- `M` `images/screenshots/aifs-main-window-win.png`
- `M` `images/screenshots/aifs-review-dialog-win.png`
- `M` `images/screenshots/llm-select-win.png`
- `M` `images/screenshots/sort-confirm-moved-win.png`

## What changed from what, why, and how
The commit updated screenshot assets in `app/include/external/llama.cpp`, `external/Catch2`, `images/screenshots/aifs-benchmark-dialog-win.png`, `images/screenshots/aifs-main-window-win.png`, `images/screenshots/aifs-review-dialog-win.png`, `images/screenshots/llm-select-win.png`, `images/screenshots/sort-confirm-moved-win.png`. The repository moved from older UI imagery to newer captures that represented the current interface more accurately.

Before this commit, the repository reflected the state immediately preceding `b88eb67`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
+Subproject commit 0dfcd3b60755bf9cb3c9ec726a584b8a4f20239b
diff --git a/external/Catch2 b/external/Catch2
index b59f4f3..de7e863 160000
--- a/external/Catch2
+++ b/external/Catch2
@@ -1 +1 @@
-Subproject commit b59f4f352291a89c4a3fe1488eeccbddeea85625
+Subproject commit de7e8630134f46e67a6b59269436f9cab94cd28e
```

The excerpt is taken from the commit diff for `chore(screenshots): update`. The most relevant surfaces are `app/include/external/llama.cpp`, `external/Catch2`, `images/screenshots/aifs-benchmark-dialog-win.png`, `images/screenshots/aifs-main-window-win.png`, `images/screenshots/aifs-review-dialog-win.png`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
