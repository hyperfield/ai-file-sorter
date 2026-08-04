# 2026-01-19: chore(readme): update

## Covered commits
- `eb379de` `2026-01-19` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `eb379de`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -187,8 +187,10 @@ File categorization with local LLMs is completely free of charge. If you prefer
    ```bash
    sudo apt update && sudo apt install -y \
      libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread \
-     libvulkan1 mesa-vulkan-drivers glslc
+     libvulkan1 mesa-vulkan-drivers glslang-tools
    ```
+   `glslc` is a binary provided by `glslang-tools` (or `shaderc` on some distros).
+   On Debian 13, use `libjsoncpp26` (and `libcurl4t64` if `libcurl4` is not available).
    Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
    GPU acceleration additionally requires either a working Vulkan 1.2+ stack (Mesa, AMD/Intel/NVIDIA drivers) or, for NVIDIA users, the matching CUDA runtime (`nvidia-cuda-toolkit` or vendor packages). The launcher automatically prefers Vulkan when both are present and falls back to CPU if neither is available.
 2. **Install the package**
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
