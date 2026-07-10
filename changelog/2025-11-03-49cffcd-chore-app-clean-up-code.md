# 2025-11-03: chore(app): clean-up code

## Covered commits
- `49cffcd` `2025-11-03` `chore(app): clean-up code`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/build_windows.ps1`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/MovableCategorizedFile.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/MovableCategorizedFile.cpp`
- `M` `app/lib/Updater.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `README.md`, `app/build_windows.ps1`, `app/include/LLMDownloader.hpp`, `app/include/MovableCategorizedFile.hpp`, `app/include/external/llama.cpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LLMDownloader.cpp`, `app/lib/MovableCategorizedFile.cpp`, and 2 more file(s). It changed the repository support state, metadata, or supporting files in the way described by `chore(app): clean-up code`.

Before this commit, the repository reflected the state immediately preceding `49cffcd`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -137,7 +137,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
 1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
    ```bash
    sudo apt update && sudo apt install -y \
-     libqt6widgets6 libcurl4 libjsoncpp25 libfmt8 libopenblas0-pthread
+    libqt6widgets6 libcurl4 libjsoncpp25 libfmt8 libopenblas0-pthread
    ```
    Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
    GPU acceleration additionally requires an NVIDIA driver with the matching CUDA runtime (`nvidia-cuda-toolkit` or the vendor packages).
diff --git a/app/build_windows.ps1 b/app/build_windows.ps1
index a096758..e1df6e8 100644
--- a/app/build_windows.ps1
+++ b/app/build_windows.ps1
@@ -110,7 +110,7 @@ if (-not (Test-Path $toolchainFile)) {
 }
 
 if ($Clean -and (Test-Path $buildDir)) {
-    Write-Host "Removing existing build directory '$buildDir'..."
+    Write-Output "Removing existing build directory '$buildDir'..."
     Remove-Item -Recurse -Force $buildDir
 }
```

The excerpt is taken from the commit diff for `chore(app): clean-up code`. The most relevant surfaces are `README.md`, `app/build_windows.ps1`, `app/include/LLMDownloader.hpp`, `app/include/MovableCategorizedFile.hpp`, `app/include/external/llama.cpp`, and 5 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
