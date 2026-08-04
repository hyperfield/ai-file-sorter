# 2025-11-07: chore(readme): update

## Covered commits
- `350e335` `2025-11-07` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `350e335`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -3,6 +3,10 @@
 
 [![Version](https://badgen.net/badge/version/1.1.0/blue)](#)
 
+<p align="center">
+  <img src="app/resources/images/icon_256x256.png" alt="AI File Sorter logo" width="128" height="128">
+</p>
+
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.  
 
 It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, and taxonomy.  
@@ -166,17 +170,18 @@ File categorization with local LLMs is completely free of charge. If you prefer
      ```bash
      sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog
      ```
-     Optional CUDA support also requires the distro CUDA packages.
+     Optional GPU acceleration also requires the distro CUDA packages (for NVIDIA) or a Vulkan 1.2+ driver/runtime (for AMD/Intel/NVIDIA).
 2. **Clone the repository**
    ```bash
    git clone https://github.com/hyperfield/ai-file-sorter.git
    cd ai-file-sorter
    git submodule update --init --recursive --remote
    ```
-3. **Build the llama runtime** (add `cuda=on` if you have a CUDA toolchain)
+3. **Build the llama runtime** (choose exactly one accelerator flag per run)
    ```bash
-   ./app/scripts/build_llama_linux.sh [cuda=on|cuda=off]
+   ./app/scripts/build_llama_linux.sh [cuda=on|off] [vulkan=on|off]
    ```
+   Use `cuda=on` when the NVIDIA CUDA toolkit is present, `vulkan=on` when your Mesa/driver stack exposes Vulkan 1.2+, or leave both off for a CPU/OpenBLAS-only build. (The script errors out if both accelerators are requested simultaneously.) For Vulkan builds, install the vendor driver or Mesa packages that provide `vulkaninfo` (e.g. `sudo apt install mesa-vulkan-drivers vulkan-tools`) and verify `vulkaninfo` succeeds before running the script; the generated libraries are staged under `app/lib/precompiled/vulkan` and `app/lib/ggml/wvulkan`.
 4. **Compile the application**
    ```bash
    cd app
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
