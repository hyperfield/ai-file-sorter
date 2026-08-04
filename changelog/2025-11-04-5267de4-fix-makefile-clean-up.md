# 2025-11-04: fix(makefile): clean up

## Covered commits
- `5267de4` `2025-11-04` `fix(makefile): clean up`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/Makefile`
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit corrected behavior in `README.md`, `app/Makefile`, `app/include/external/llama.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(makefile): clean up`.

Before this commit, the repository reflected the state immediately preceding `5267de4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -152,14 +152,14 @@ File categorization with local LLMs is completely free of charge. If you prefer
 1. **Install dependencies**
    - Debian / Ubuntu:
      ```bash
-     sudo apt update && sudo apt install -y \\
-       build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-tools-dev-tools \\
-       libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
+    sudo apt update && sudo apt install -y \
+      build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-tools-dev-tools \
+      libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
      ```
    - Fedora / RHEL:
      ```bash
-     sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \\
-       libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
+    sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
+      libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
      ```
    - Arch / Manjaro:
      ```bash
diff --git a/app/Makefile b/app/Makefile
index 774d3a4..feb2ab3 100644
--- a/app/Makefile
+++ b/app/Makefile
@@ -69,6 +69,8 @@ WRAPPED_BINARY := $(notdir $(TARGET))
 # Compiler and flags
 CXX = g++
 CXXFLAGS += -std=c++20 -Wall -O2 -fPIC
+# Suppress false positives from fmt/Qt headers (see GCC 13 + fmt bigint warnings)
+CXXFLAGS += -Wno-array-bounds
 INCLUDE_DIRS = -I./include -I./include/llama
 LIB_DIRS =
 ifeq ($(UNAME), Linux)
```

The excerpt is taken from the commit diff for `fix(makefile): clean up`. The most relevant surfaces are `README.md`, `app/Makefile`, `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
