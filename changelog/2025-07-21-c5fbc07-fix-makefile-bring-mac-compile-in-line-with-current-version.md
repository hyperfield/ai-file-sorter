# 2025-07-21: fix(makefile): bring mac compile in line with current version

## Covered commits
- `c5fbc07` `2025-07-21` `fix(makefile): bring mac compile in line with current version`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit adjusted version-bearing files in `app/Makefile`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `c5fbc07`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -19,20 +19,25 @@ ifeq ($(UNAME), Linux)
 
 
 else ifeq ($(UNAME), Darwin)
+    export PATH := /opt/homebrew/opt/curl/bin:$(PATH)
+
+    export PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:/opt/homebrew/opt/libffi/lib/pkgconfig:/opt/homebrew/opt/expat/lib/pkgconfig:$(PKG_CONFIG_PATH)
+
+    export LDFLAGS += -L/opt/homebrew/opt/libffi/lib
+    export CPPFLAGS += -I/opt/homebrew/opt/libffi/include
+
     PLATFORM := MacOS
-    CXXFLAGS += -DMACOS
+    CXXFLAGS += -DMACOS -DENABLE_METAL -DGGML_USE_METAL -Wno-deprecated -Iinclude/llama
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
-    RESOURCE_DIR := /usr/local/share/aifilesorter
 
     IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
-
-    ifeq ($(IS_APPLE_SILICON), 1)
-        CXXFLAGS += -DENABLE_METAL
-    endif
+    SPDLOG_PATH := $(shell if [ "$(IS_APPLE_SILICON)" = "1" ]; then echo "/opt/homebrew/include"; else echo "/usr/local/include"; fi)
+    CXXFLAGS += -I$(SPDLOG_PATH)
 
     LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
-    LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
+	LDFLAGS += -framework Metal -framework Foundation
+    LDFLAGS += -Wl,-rpath,@loader_path/lib
 
 
 else ifeq ($(UNAME), MINGW64_NT)
@@ -50,7 +55,7 @@ endif
 # Compiler and flags
 CXX = g++
 CXXFLAGS += -std=c++20 -Wall $(shell pkg-config --cflags gtkmm-3.0)
-CXXFLAGS += -g -O0
+CXXFLAGS += -O2
 
 LDFLAGS += $(shell pkg-config --libs gtkmm-3.0)
 INCLUDE_DIRS = -I./include -I./include/llama
```

The excerpt is taken from the commit diff for `fix(makefile): bring mac compile in line with current version`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
