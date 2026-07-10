# 2025-11-22: chore(screenshots): update

## Covered commits
- `9664256` `2025-11-22` `chore(screenshots): update`

## Motivation
This commit refreshed visual documentation so the repository UI captures matched the actual application behavior and appearance. That matters because screenshots are part of release communication, support, and product explanation.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `images/screenshots/aifs-main-window-win.png`
- `M` `images/screenshots/aifs-review-dialog-win.png`
- `A` `images/screenshots/sort-confirm-moved.png`

## What changed from what, why, and how
The commit updated screenshot assets in `app/include/external/llama.cpp`, `images/screenshots/aifs-main-window-win.png`, `images/screenshots/aifs-review-dialog-win.png`, `images/screenshots/sort-confirm-moved.png`. The repository moved from older UI imagery to newer captures that represented the current interface more accurately.

Before this commit, the repository reflected the state immediately preceding `9664256`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 3f3a4fb9c3b907c68598363b204e6f58f4757c8c
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
diff --git a/images/screenshots/aifs-main-window-win.png b/images/screenshots/aifs-main-window-win.png
index 83f47e4..f57ad08 100644
Binary files a/images/screenshots/aifs-main-window-win.png and b/images/screenshots/aifs-main-window-win.png differ
diff --git a/images/screenshots/aifs-review-dialog-win.png b/images/screenshots/aifs-review-dialog-win.png
index 931e357..953c0cb 100644
Binary files a/images/screenshots/aifs-review-dialog-win.png and b/images/screenshots/aifs-review-dialog-win.png differ
diff --git a/images/screenshots/sort-confirm-moved.png b/images/screenshots/sort-confirm-moved.png
new file mode 100644
index 0000000..223e6f8
Binary files /dev/null and b/images/screenshots/sort-confirm-moved.png differ
```

The excerpt is taken from the commit diff for `chore(screenshots): update`. The most relevant surfaces are `app/include/external/llama.cpp`, `images/screenshots/aifs-main-window-win.png`, `images/screenshots/aifs-review-dialog-win.png`, `images/screenshots/sort-confirm-moved.png`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
