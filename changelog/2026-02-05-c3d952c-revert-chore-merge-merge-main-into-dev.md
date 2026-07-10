# 2026-02-05: Revert "chore(merge): merge main into dev"

## Covered commits
- `c3d952c` `2026-02-05` `Revert "chore(merge): merge main into dev"`

## Motivation
This revert commit was made because a previous change turned out not to be the desired shipped state. Reverting is a deliberate correction to restore a better baseline quickly.

## Commit message body
This reverts commit 227caa5e20ef6689385dc91143a509524eb81af8, reversing
changes made to aada3252c8bc683bb88513303d57620d283b5a8d.

## Files changed
- `M` `README.md`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/GeminiClient.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `external/Catch2`

## What changed from what, why, and how
The commit rolled back an earlier change affecting `README.md`, `app/include/external/llama.cpp`, `app/lib/GeminiClient.cpp`, `app/lib/Settings.cpp`, `external/Catch2`. It moved the repository from the undesired intermediate state back toward the previously working or preferred behavior.

Before this commit, the repository reflected the state immediately preceding `c3d952c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -267,8 +267,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
      ```bash
      sudo apt update && sudo apt install -y \
        build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-tools-dev-tools \
-       libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev \
-       libopenblas-dev libvulkan-dev patchelf
+       libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
      ```
    - Fedora / RHEL:
 
@@ -280,28 +279,13 @@ File categorization with local LLMs is completely free of charge. If you prefer
 
      ```bash
      sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
-       libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel openblas-devel \
-       vulkan-headers vulkan-loader-devel patchelf
-     ```
-
-   - openSUSE (Leap / Tumbleweed):
-
-     ```bash
-     sudo zypper install -y gcc-c++ cmake git qt6-base-devel qt6-tools-devel \
-       libcurl-devel jsoncpp-devel sqlite3-devel libopenssl-devel fmt-devel spdlog-devel openblas-devel \
-       vulkan-headers vulkan-devel shaderc patchelf
-     ```
-
-     If `rcc` is not found, openSUSE can expose it via `qtpaths6`:
-
-     ```bash
-     export PATH="$(qtpaths6 --query QT_HOST_LIBEXECS):$PATH"
+       libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
      ```
 
    - Arch / Manjaro:
 
      ```bash
-     sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog openblas vulkan-headers patchelf
+     sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog
      ```
 
      Optional GPU acceleration also requires either the distro Vulkan 1.2+ driver/runtime (Mesa, AMD, Intel, NVIDIA) or CUDA packages for NVIDIA cards. Install whichever stack you plan to use; the app will fall back to CPU automatically if none are detected.
```

The excerpt is taken from the commit diff for `Revert "chore(merge): merge main into dev"`. The most relevant surfaces are `README.md`, `app/include/external/llama.cpp`, `app/lib/GeminiClient.cpp`, `app/lib/Settings.cpp`, `external/Catch2`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
