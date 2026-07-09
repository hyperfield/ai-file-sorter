# 2025-07-02: fix(windows): adjust Makefile and LLaMa build config; fix cURL cert path handling

## Covered commits
- `e532e26` `2025-07-02` `fix(windows): adjust Makefile and LLaMa build config; fix cURL cert path handling`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/Updater.cpp`
- `M` `app/resources/resources.c`
- `M` `app/resources/resources.gresource`
- `M` `app/scripts/build_llama_windows.sh`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`, `app/lib/LLMDownloader.cpp`, `app/lib/Updater.cpp`, `app/resources/resources.c`, `app/resources/resources.gresource`, `app/scripts/build_llama_windows.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(windows): adjust Makefile and LLaMa build config; fix cURL cert path handling`.

Before this commit, the repository reflected the state immediately preceding `e532e26`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -26,7 +26,7 @@ else ifeq ($(UNAME), Darwin)
     IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
 
     ifeq ($(IS_APPLE_SILICON), 1)
-        # CXXFLAGS += -DENABLE_METAL
+        CXXFLAGS += -DENABLE_METAL
     endif
 
     LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
@@ -37,8 +37,8 @@ else ifeq ($(UNAME), MINGW64_NT)
     PLATFORM := Windows (64-bit)
     CXXFLAGS += -DWINDOWS
     TARGET := $(BIN_DIR)/AiFileSorter.exe
-    INSTALL_DIR := C:/Program\ Files/AiFileSorter
-    RESOURCE_DIR := C:/Program\ Files/AiFileSorter/resources
+	INSTALL_DIR := "C:/Program Files/Ai File Sorter"
+	RESOURCE_DIR := "C:/Program Files/Ai File Sorter/resources"
     WINDRES = windres
     RC_FILE = resources/exe_icon.rc
     RC_OBJ = resources/exe_icon.o
```

The excerpt is taken from the commit diff for `fix(windows): adjust Makefile and LLaMa build config; fix cURL cert path handling`. The most relevant surfaces are `app/Makefile`, `app/lib/LLMDownloader.cpp`, `app/lib/Updater.cpp`, `app/resources/resources.c`, `app/resources/resources.gresource`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
