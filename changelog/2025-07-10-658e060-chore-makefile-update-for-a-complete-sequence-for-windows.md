# 2025-07-10: chore(makefile): update for a complete  sequence for windows

## Covered commits
- `658e060` `2025-07-10` `chore(makefile): update for a complete  sequence for windows`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/Makefile`. It changed the repository support state, metadata, or supporting files in the way described by `chore(makefile): update for a complete  sequence for windows`.

Before this commit, the repository reflected the state immediately preceding `658e060`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -38,13 +38,11 @@ else ifeq ($(UNAME), MINGW64_NT)
     CXXFLAGS += -DWINDOWS
     TARGET := $(BIN_DIR)/AiFileSorter.exe
 	INSTALL_DIR := "C:/Program Files/Ai File Sorter"
-	RESOURCE_DIR := "C:/Program Files/Ai File Sorter/resources"
     WINDRES = windres
     RC_FILE = resources/exe_icon.rc
     RC_OBJ = resources/exe_icon.o
 
-    # LDFLAGS += -mwindows -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
-	LDFLAGS += -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
+    LDFLAGS += -mwindows -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
 endif
 
 # Compiler and flags
@@ -90,33 +88,56 @@ clean:
 install: $(TARGET)
 ifeq ($(PLATFORM), Linux)
 	mkdir -p $(INSTALL_DIR)
-	mkdir -p $(RESOURCE_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
-	cp -r resources/* $(RESOURCE_DIR)
 else ifeq ($(PLATFORM), MacOS)
 	mkdir -p $(INSTALL_DIR)
-	mkdir -p $(RESOURCE_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
-	cp -r resources/* $(RESOURCE_DIR)
-else ifeq ($(PLATFORM), Windows (32-bit))
-	mkdir -p $(INSTALL_DIR)
-	mkdir -p $(RESOURCE_DIR)
-	cp $(TARGET) $(INSTALL_DIR)/aifilesorter.exe
-	cp -r resources/* $(RESOURCE_DIR)
 else ifeq ($(PLATFORM), Windows (64-bit))
-	mkdir -p $(INSTALL_DIR)
-	mkdir -p $(RESOURCE_DIR)
-	cp $(TARGET) $(INSTALL_DIR)/aifilesorter.exe
-	cp -r resources/* $(RESOURCE_DIR)
+	# Create necessary directories
+	mkdir -p "$(INSTALL_DIR)"/certs
+	mkdir -p "$(INSTALL_DIR)"/gdk-pixbuf-2.0
+	mkdir -p "$(INSTALL_DIR)"/schemas
+	mkdir -p "$(INSTALL_DIR)"/share/icons
+	mkdir -p "$(INSTALL_DIR)"/share/themes
+
+	# Define DLLs and other required files
+	INSTALL_FILES := \
+		$(TARGET) \
+		$(wildcard ./lib/precompiled/*.dll) \
+		C:/msys64/mingw64/bin/gdbus.exe \
+		$(foreach dll, \
+			libcairo-2 libfribidi-0 libharfbuzz-0 libpango-1.0-0 libssl-3-x64 \
+			libcairo-gobject-2 libgcc_s_seh-1 libiconv-2 libpangocairo-1.0-0 \
+			libstdc++-6 libcrypto-3-x64 libgdk-3-0 libidn2-0 libpangoft2-1.0-0 \
+			libthai-0 libtiff-6 libcurl-4 libdatrie-1 libgiomm-2.4-1 libjpeg-8 \
+			libpcre2-8-0 libpangowin32-1.0-0 libunistring-5 libdeflate libepoxy-0 \
+			libglib-2.0-0 libjsoncpp-26 libpng16-16 libwinpthread-1 libLerc libexpat-1 \
+			libglibmm-2.4-1 liblzma-5 libpsl-5 libatk-1.0-0 libffi-8 libgmodule-2.0-0 \
+			libnghttp2-14 libsharpyuv-0 libbrotlicommon libfmt-11 libgobject-2.0-0 \
+			libnghttp3-9 libsigc-2.0-0 zlib1 libbrotlidec libfontconfig-1 libgraphite2 \
+			libngtcp2-16 libsqlite3-0 libbz2-1 libfreetype-6 libgtk-3-0 libngtcp2_crypto_ossl \
+			libssh2-1, \
+			C:/msys64/mingw64/bin/$(dll).dll)
+
+	# Copy executable and DLLs
+	cp -v $(INSTALL_FILES) "$(INSTALL_DIR)/"
+
+	# Copy other required resources
+	cp -v C:/msys64/usr/ssl/cert.pem "$(INSTALL_DIR)/certs/cacert.pem"
+	cp -vr C:/msys64/mingw64/lib/gdk-pixbuf-2.0/* "$(INSTALL_DIR)/gdk-pixbuf-2.0/"
+	cp -vr C:/msys64/mingw64/share/glib-2.0/schemas/* "$(INSTALL_DIR)/schemas/"
+	cp -vr C:/msys64/mingw64/share/icons/* "$(INSTALL_DIR)/share/icons/"
+	cp -vr C:/msys64/mingw64/share/themes/* "$(INSTALL_DIR)/share/themes/"
+
+	# Compile schemas
+	glib-compile-schemas "$(INSTALL_DIR)/schemas"
 endif
 
 uninstall:
 	ifeq ($(PLATFORM), Linux)
 		rm -f $(INSTALL_DIR)/aifilesorter
-		rm -rf $(RESOURCE_DIR)
 	else ifeq ($(PLATFORM), MacOS)
 		rm -f $(INSTALL_DIR)/aifilesorter
-		rm -rf $(RESOURCE_DIR)
 	else ifeq ($(PLATFORM), Windows (32-bit))
 		rm -rf $(INSTALL_DIR)
 	else ifeq ($(PLATFORM), Windows (64-bit))
```

The excerpt is taken from the commit diff for `chore(makefile): update for a complete  sequence for windows`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
