# 2025-11-04: chore(code): cleanup

## Covered commits
- `9419b58` `2025-11-04` `chore(code): cleanup`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/Makefile`
- `M` `app/include/app_version.hpp`
- `M` `app/include/constants.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `README.md`, `app/Makefile`, `app/include/app_version.hpp`, `app/include/constants.hpp`, `app/include/external/llama.cpp`, `app/main.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `9419b58`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,7 +1,7 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/1.0.0/green)](#)
+[![Version](https://badgen.net/badge/version/1.0.5/blue)](#)
 
 AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.  
 
@@ -137,7 +137,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
 1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
    ```bash
    sudo apt update && sudo apt install -y \
-    libqt6widgets6 libcurl4 libjsoncpp25 libfmt8 libopenblas0-pthread
+     libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread
    ```
    Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
    GPU acceleration additionally requires an NVIDIA driver with the matching CUDA runtime (`nvidia-cuda-toolkit` or the vendor packages).
```

The excerpt is taken from the commit diff for `chore(code): cleanup`. The most relevant surfaces are `README.md`, `app/Makefile`, `app/include/app_version.hpp`, `app/include/constants.hpp`, `app/include/external/llama.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
