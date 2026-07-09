# 2025-06-26: chore(makefile): adjust flags and paths for Windows and macOS compatibility

## Covered commits
- `049988c` `2025-06-26` `chore(makefile): adjust flags and paths for Windows and macOS compatibility`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/Makefile`. It changed the repository support state, metadata, or supporting files in the way described by `chore(makefile): adjust flags and paths for Windows and macOS compatibility`.

Before this commit, the repository reflected the state immediately preceding `049988c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -8,15 +8,10 @@ RESOURCES = resources/resources.c
 
 ifeq ($(UNAME), Linux)
     PLATFORM := Linux
-    CXXFLAGS += -DLINUX -DENABLE_CUDA -DENABLE_OPENCL
+    CXXFLAGS += -DLINUX
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
     RESOURCE_DIR := /usr/local/share/aifilesorter
-    # LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lcommon \
-	# 	-lggml -lggml-cpu -lggml-cuda -lggml-base -lggml-blas \
-  	# 	-lopenblas -lcudart -lcuda -lcublas \
-  	# 	-pthread -fopenmp
-
 	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
 	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
 
@@ -27,38 +22,29 @@ else ifeq ($(UNAME), Darwin)
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
     RESOURCE_DIR := /usr/local/share/aifilesorter
-    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl
 
     IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
 
     ifeq ($(IS_APPLE_SILICON), 1)
-        CXXFLAGS += -DENABLE_METAL
-    else
-        CXXFLAGS += -DENABLE_OPENCL
+        # CXXFLAGS += -DENABLE_METAL
     endif
 
-else ifeq ($(UNAME), MINGW32_NT)
-    PLATFORM := Windows (32-bit)
-    CXXFLAGS += -DWINDOWS -DENABLE_OPENCL
-    TARGET := $(BIN_DIR)/AI\ File\ Sorter.exe
-    INSTALL_DIR := C:/Program\ Files\ (x86)/AiFileSorter
-    RESOURCE_DIR := C:/Program\ Files\ (x86)/AiFileSorter/resources
-    WINDRES = windres
-    RC_FILE = resources/exe_icon.rc
-    RC_OBJ = resources/exe_icon.o
-    LDFLAGS += -mwindows
+    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
+    LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
+
 
 else ifeq ($(UNAME), MINGW64_NT)
     PLATFORM := Windows (64-bit)
-    CXXFLAGS += -DWINDOWS -DENABLE_OPENCL
+    CXXFLAGS += -DWINDOWS
     TARGET := $(BIN_DIR)/AiFileSorter.exe
     INSTALL_DIR := C:/Program\ Files/AiFileSorter
     RESOURCE_DIR := C:/Program\ Files/AiFileSorter/resources
     WINDRES = windres
     RC_FILE = resources/exe_icon.rc
     RC_OBJ = resources/exe_icon.o
-    LDFLAGS += -mwindows -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt
-endif
+
+    LDFLAGS += -mwindows -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
+
 
 # Compiler and flags
 CXX = g++
```

The excerpt is taken from the commit diff for `chore(makefile): adjust flags and paths for Windows and macOS compatibility`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
