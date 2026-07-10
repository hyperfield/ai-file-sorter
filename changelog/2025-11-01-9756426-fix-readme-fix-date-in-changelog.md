# 2025-11-01: fix(readme): fix date in changelog

## Covered commits
- `9756426` `2025-11-01` `fix(readme): fix date in changelog`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `9756426`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -118,8 +118,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
 1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
    ```bash
    sudo apt update && sudo apt install -y \
-     libqt6widgets6 libqt6gui6 libqt6core6 libqt6dbus6 \
-     libcurl4 libjsoncpp25 libfmt8 libopenblas0-pthread
+     libqt6widgets6 libcurl4 libjsoncpp25 libfmt8 libopenblas0-pthread
    ```
    Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
    GPU acceleration additionally requires an NVIDIA driver with the matching CUDA runtime (`nvidia-cuda-toolkit` or the vendor packages).
```

The excerpt is taken from the commit diff for `fix(readme): fix date in changelog`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
