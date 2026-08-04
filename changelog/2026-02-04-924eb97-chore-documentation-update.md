# 2026-02-04: chore(documentation): update

## Covered commits
- `924eb97` `2026-02-04` `chore(documentation): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `924eb97`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -238,13 +238,20 @@ File categorization with local LLMs is completely free of charge. If you prefer
 #### Prebuilt Debian/Ubuntu package
 
 1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
-   ```bash
-   sudo apt update && sudo apt install -y \
-     libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread \
-     libvulkan1 mesa-vulkan-drivers glslang-tools
-   ```
-   `glslc` is a binary provided by `glslang-tools` (or `shaderc` on some distros).
-   On Debian 13, use `libjsoncpp26` (and `libcurl4t64` if `libcurl4` is not available).
+   - Ubuntu 24.04 / Debian 12:
+     ```bash
+     sudo apt update && sudo apt install -y \
+       libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread \
+       libvulkan1 mesa-vulkan-drivers patchelf
+     ```
+   - Debian 13 (trixie):
+     ```bash
+     sudo apt update && sudo apt install -y \
+       libqt6widgets6 libcurl4t64 libjsoncpp26 libfmt10 libopenblas0-pthread \
+       libvulkan1 mesa-vulkan-drivers patchelf
+     ```
+   If you build the Vulkan backend from source, install `glslc` (Debian/Ubuntu package: `glslc`; on some distros: `shaderc` or `shaderc-tools`).
+   On Debian 13, use `libjsoncpp26`, `libfmt10`, and `libcurl4t64` (APT may auto-select `libcurl4t64` if `libcurl4` is not available).
    Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
    GPU acceleration additionally requires either a working Vulkan 1.2+ stack (Mesa, AMD/Intel/NVIDIA drivers) or, for NVIDIA users, the matching CUDA runtime (`nvidia-cuda-toolkit` or vendor packages). The launcher automatically prefers Vulkan when both are present and falls back to CPU if neither is available.
 2. **Install the package**
@@ -320,7 +327,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
    ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
    # CUDA (optional; requires NVIDIA driver + CUDA toolkit)
    ./app/scripts/build_llama_linux.sh cuda=on vulkan=off
-   # Vulkan (optional; requires a working Vulkan 1.2+ stack, e.g. mesa-vulkan-drivers + vulkan-tools)
+   # Vulkan (optional; requires a working Vulkan 1.2+ stack and glslc, e.g. mesa-vulkan-drivers + vulkan-tools + glslc)
    ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
    ```
```

The excerpt is taken from the commit diff for `chore(documentation): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
