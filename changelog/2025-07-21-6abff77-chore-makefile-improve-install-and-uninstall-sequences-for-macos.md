# 2025-07-21: chore(makefile): improve install and uninstall sequences for macOS

## Covered commits
- `6abff77` `2025-07-21` `chore(makefile): improve install and uninstall sequences for macOS`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/Makefile`. It changed the repository support state, metadata, or supporting files in the way described by `chore(makefile): improve install and uninstall sequences for macOS`.

Before this commit, the repository reflected the state immediately preceding `6abff77`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -30,6 +30,7 @@ else ifeq ($(UNAME), Darwin)
     CXXFLAGS += -DMACOS -DENABLE_METAL -DGGML_USE_METAL -Wno-deprecated -Iinclude/llama
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
+	INSTALL_LIB_DIR := /usr/local/lib
 
     IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
     SPDLOG_PATH := $(shell if [ "$(IS_APPLE_SILICON)" = "1" ]; then echo "/opt/homebrew/include"; else echo "/usr/local/include"; fi)
@@ -109,9 +110,24 @@ ifeq ($(PLATFORM), Linux)
 	@echo "Installation complete."
 
 else ifeq ($(PLATFORM), MacOS)
+	@echo "Installing binary to $(INSTALL_DIR)..."
 	mkdir -p $(INSTALL_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
 
+	@echo "Installing libraries to $(INSTALL_LIB_DIR)..."
+	mkdir -p $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libggml-base.dylib $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libggml-blas.dylib $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libggml-cpu.dylib $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libggml-metal.dylib $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libggml.dylib $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libmtmd.dylib $(INSTALL_LIB_DIR)
+	cp lib/precompiled/libllama.dylib $(INSTALL_LIB_DIR)
+
+	install_name_tool -add_rpath $(INSTALL_LIB_DIR) $(INSTALL_DIR)/aifilesorter
+
+	@echo "macOS installation complete."
+
 else ifeq ($(PLATFORM), Windows (64-bit))
 	mkdir -p $(INSTALL_DIR)/certs
 	mkdir -p $(INSTALL_DIR)/share
```

The excerpt is taken from the commit diff for `chore(makefile): improve install and uninstall sequences for macOS`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
