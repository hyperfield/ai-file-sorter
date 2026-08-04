# 2025-11-09: fix(misc): misc. fixes and updates

## Covered commits
- `96f909d` `2025-11-09` `fix(misc): misc. fixes and updates`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/CMakeLists.txt`
- `M` `app/build_windows.ps1`
- `M` `app/include/LocalLLMTestAccess.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MainAppUiBuilder.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/main.cpp`
- `M` `app/scripts/build_llama_windows.ps1`
- `M` `app/startapp_windows.cpp`
- `M` `tests/unit/test_categorization_dialog.cpp`
- `M` `tests/unit/test_file_scanner.cpp`
- `M` `tests/unit/test_main_app_translation.cpp`
- `M` `tests/unit/test_support_prompt.cpp`

## What changed from what, why, and how
The commit corrected behavior in `README.md`, `app/CMakeLists.txt`, `app/build_windows.ps1`, `app/include/LocalLLMTestAccess.hpp`, `app/include/MainApp.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `app/lib/MainAppUiBuilder.cpp`, and 8 more file(s). It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(misc): misc. fixes and updates`.

Before this commit, the repository reflected the state immediately preceding `96f909d`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -134,7 +134,7 @@ AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* an
 - **Compiler**: A C++20-capable compiler (`g++` or `clang++`).
 - **Qt 6**: Core, Gui, Widgets modules and the Qt resource compiler (`qt6-base-dev` / `qt6-tools` on Linux, `brew install qt` on macOS).
 - **Libraries**: `curl`, `sqlite3`, `fmt`, `spdlog`, and the prebuilt `llama` libraries shipped under `app/lib/precompiled`.
-- **Optional GPU backends**: CUDA 12.x for NVIDIA cards, or a Vulkan 1.2+ driver for AMD/NVIDIA/Intel GPUs. If neither is present, the app falls back to CPU/OpenBLAS automatically.
+- **Optional GPU backends**: A Vulkan 1.2+ runtime (preferred) or CUDA 12.x for NVIDIA cards. `StartAiFileSorter.exe`/`run_aifilesorter.sh` auto-detect the best available backend and fall back to CPU/OpenBLAS automatically, so CUDA is never required to run the app.
 - **Git** (optional): For cloning this repository. Archives can also be downloaded.
 - **OpenAI API Key** (optional): Required only when using the remote ChatGPT workflow.
 
@@ -154,7 +154,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
      libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread
    ```
    Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
-   GPU acceleration additionally requires an NVIDIA driver with the matching CUDA runtime (`nvidia-cuda-toolkit` or the vendor packages).
+   GPU acceleration additionally requires either a working Vulkan 1.2+ stack (Mesa, AMD/Intel/NVIDIA drivers) or, for NVIDIA users, the matching CUDA runtime (`nvidia-cuda-toolkit` or vendor packages). The launcher automatically prefers Vulkan when both are present and falls back to CPU if neither is available.
 2. **Install the package**
    ```bash
    sudo apt install ./aifilesorter_1.0.0_amd64.deb
```

The excerpt is taken from the commit diff for `fix(misc): misc. fixes and updates`. The most relevant surfaces are `README.md`, `app/CMakeLists.txt`, `app/build_windows.ps1`, `app/include/LocalLLMTestAccess.hpp`, `app/include/MainApp.hpp`, and 11 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
