# 2025-11-08: fix(compile): on macOS

## Covered commits
- `508db54` `2025-11-08` `fix(compile): on macOS`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`, `app/include/external/llama.cpp`, `app/lib/LocalLLMClient.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(compile): on macOS`.

Before this commit, the repository reflected the state immediately preceding `508db54`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -69,8 +69,13 @@ WRAPPED_BINARY := $(notdir $(TARGET))
 # Compiler and flags
 CXX = g++
 CXXFLAGS += -std=c++20 -Wall -O2 -fPIC
-# Suppress GCC 13 fmt/Qt false positives
+CXX_VERSION_INFO := $(shell $(CXX) --version 2>/dev/null)
+# Suppress GCC-only diagnostics; clang (macOS) does not support some of them
+ifneq (,$(findstring clang,$(CXX_VERSION_INFO)))
+CXXFLAGS += -Wno-array-bounds
+else
 CXXFLAGS += -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
+endif
 INCLUDE_DIRS = -I./include -I./include/llama
 LIB_DIRS =
 ifeq ($(UNAME), Linux)
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index ee09828..7675c55 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ee09828cb057460b369576410601a3a09279e23c
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
```

The excerpt is taken from the commit diff for `fix(compile): on macOS`. The most relevant surfaces are `app/Makefile`, `app/include/external/llama.cpp`, `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
