# 2025-07-17: chore(makefile): tailor install and uninstall rules for Linux

## Covered commits
- `2c5ee4f` `2025-07-17` `chore(makefile): tailor install and uninstall rules for Linux`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/Makefile`. It changed the repository support state, metadata, or supporting files in the way described by `chore(makefile): tailor install and uninstall rules for Linux`.

Before this commit, the repository reflected the state immediately preceding `2c5ee4f`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -11,7 +11,9 @@ ifeq ($(UNAME), Linux)
     CXXFLAGS += -DLINUX
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
-    RESOURCE_DIR := /usr/local/share/aifilesorter
+    INSTALL_LIB_DIR := /usr/local/lib/aifilesorter
+	LD_CONF_FILE := /etc/ld.so.conf.d/aifilesorter.conf
+
 	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
 	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
 
@@ -88,9 +90,20 @@ clean:
 install: $(TARGET)
 
 ifeq ($(PLATFORM), Linux)
+	@echo "Installing binary to $(INSTALL_DIR)..."
 	mkdir -p $(INSTALL_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
 
+	@echo "Installing libraries to $(INSTALL_LIB_DIR)..."
+	mkdir -p $(INSTALL_LIB_DIR)
+	cp lib/precompiled/*.so $(INSTALL_LIB_DIR)
+
+	@echo "Registering library path with ldconfig..."
+	echo "$(INSTALL_LIB_DIR)" > $(LD_CONF_FILE)
+	ldconfig
+
+	@echo "Installation complete."
+
 else ifeq ($(PLATFORM), MacOS)
 	mkdir -p $(INSTALL_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
```

The excerpt is taken from the commit diff for `chore(makefile): tailor install and uninstall rules for Linux`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
