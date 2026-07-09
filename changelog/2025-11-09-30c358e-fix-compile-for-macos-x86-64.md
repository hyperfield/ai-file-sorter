# 2025-11-09: fix(compile): for macOS x86_64

## Covered commits
- `30c358e` `2025-11-09` `fix(compile): for macOS x86_64`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/Makefile`
- `D` `app/lib/precompiled/vulkan/.gitkeep`
- `M` `app/scripts/build_llama_macos.sh`

## What changed from what, why, and how
The commit corrected behavior in `README.md`, `app/Makefile`, `app/lib/precompiled/vulkan/.gitkeep`, `app/scripts/build_llama_macos.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(compile): for macOS x86_64`.

Before this commit, the repository reflected the state immediately preceding `30c358e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -166,14 +166,14 @@ File categorization with local LLMs is completely free of charge. If you prefer
 1. **Install dependencies**
    - Debian / Ubuntu:
      ```bash
-    sudo apt update && sudo apt install -y \
-      build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-tools-dev-tools \
-      libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
+     sudo apt update && sudo apt install -y \
+       build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-tools-dev-tools \
+       libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
      ```
    - Fedora / RHEL:
      ```bash
-    sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
-      libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
+     sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
+       libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
      ```
    - Arch / Manjaro:
      ```bash
@@ -186,16 +186,17 @@ File categorization with local LLMs is completely free of charge. If you prefer
    cd ai-file-sorter
    git submodule update --init --recursive --remote
    ```
+   > **Submodule tip:** If you previously downloaded `llama.cpp` or Catch2 manually, remove or rename `app/include/external/llama.cpp` and `external/Catch2` before running the `git submodule` command. Git needs those directories to be empty so it can populate them with the tracked submodules.
 3. **Build the llama runtime variants** (run once per backend you plan to ship/test)
-  ```bash
-  # CPU / OpenBLAS
-  ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
-  # CUDA (optional; requires NVIDIA driver + CUDA toolkit)
-  ./app/scripts/build_llama_linux.sh cuda=on vulkan=off
-  # Vulkan (optional; requires a working Vulkan 1.2+ stack, e.g. mesa-vulkan-drivers + vulkan-tools)
-  ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
-  ```
-  Each invocation stages the corresponding `llama`/`ggml` libraries under `app/lib/precompiled/<variant>` and the runtime DLL/SO copies under `app/lib/ggml/w<variant>`. The script refuses to enable CUDA and Vulkan simultaneously, so run it separately for each backend. Shipping both directories lets the launcher pick Vulkan when available, then CUDA, and otherwise stay on CPU—no CUDA-only dependency remains.
+   ```bash
+   # CPU / OpenBLAS
+   ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
+   # CUDA (optional; requires NVIDIA driver + CUDA toolkit)
+   ./app/scripts/build_llama_linux.sh cuda=on vulkan=off
+   # Vulkan (optional; requires a working Vulkan 1.2+ stack, e.g. mesa-vulkan-drivers + vulkan-tools)
+   ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
+   ```
+   Each invocation stages the corresponding `llama`/`ggml` libraries under `app/lib/precompiled/<variant>` and the runtime DLL/SO copies under `app/lib/ggml/w<variant>`. The script refuses to enable CUDA and Vulkan simultaneously, so run it separately for each backend. Shipping both directories lets the launcher pick Vulkan when available, then CUDA, and otherwise stay on CPU—no CUDA-only dependency remains.
 4. **Compile the application**
    ```bash
    cd app
```

The excerpt is taken from the commit diff for `fix(compile): for macOS x86_64`. The most relevant surfaces are `README.md`, `app/Makefile`, `app/lib/precompiled/vulkan/.gitkeep`, `app/scripts/build_llama_macos.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
