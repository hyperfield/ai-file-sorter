# 2026-01-03: chore(readme): clean up

## Covered commits
- `ed47c2c` `2026-01-03` `chore(readme): clean up`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `ed47c2c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -179,19 +179,27 @@ File categorization with local LLMs is completely free of charge. If you prefer
      sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
        libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
      ```
+
    - Arch / Manjaro:
+
      ```bash
      sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog
      ```
+
      Optional GPU acceleration also requires either the distro Vulkan 1.2+ driver/runtime (Mesa, AMD, Intel, NVIDIA) or CUDA packages for NVIDIA cards. Install whichever stack you plan to use; the app will fall back to CPU automatically if none are detected.
+
 2. **Clone the repository**
+
    ```bash
    git clone https://github.com/hyperfield/ai-file-sorter.git
    cd ai-file-sorter
    git submodule update --init --recursive --remote
    ```
+
    > **Submodule tip:** If you previously downloaded `llama.cpp` or Catch2 manually, remove or rename `app/include/external/llama.cpp` and `external/Catch2` before running the `git submodule` command. Git needs those directories to be empty so it can populate them with the tracked submodules.
+
 3. **Build the llama runtime variants** (run once per backend you plan to ship/test)
+
    ```bash
    # CPU / OpenBLAS
    ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
@@ -200,14 +208,20 @@ File categorization with local LLMs is completely free of charge. If you prefer
    # Vulkan (optional; requires a working Vulkan 1.2+ stack, e.g. mesa-vulkan-drivers + vulkan-tools)
    ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
    ```
+
    Each invocation stages the corresponding `llama`/`ggml` libraries under `app/lib/precompiled/<variant>` and the runtime DLL/SO copies under `app/lib/ggml/w<variant>`. The script refuses to enable CUDA and Vulkan simultaneously, so run it separately for each backend. Shipping both directories lets the launcher pick Vulkan when available, then CUDA, and otherwise stay on CPU—no CUDA-only dependency remains.
+
 4. **Compile the application**
+
    ```bash
    cd app
    make -j4
    ```
+
    The binary is produced at `app/bin/aifilesorter`.
+
 5. **Install system-wide (optional)**
+
    ```bash
    sudo make install
    ```
```

The excerpt is taken from the commit diff for `chore(readme): clean up`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
