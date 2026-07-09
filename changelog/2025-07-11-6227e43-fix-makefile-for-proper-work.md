# 2025-07-11: fix(makefile): for proper  work

## Covered commits
- `6227e43` `2025-07-11` `fix(makefile): for proper  work`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(makefile): for proper  work`.

Before this commit, the repository reflected the state immediately preceding `6227e43`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -86,51 +86,45 @@ clean:
 	rm -rf $(OBJ_DIR) $(BIN_DIR) $(RC_OBJ)
 
 install: $(TARGET)
+
 ifeq ($(PLATFORM), Linux)
 	mkdir -p $(INSTALL_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
+
 else ifeq ($(PLATFORM), MacOS)
 	mkdir -p $(INSTALL_DIR)
 	cp $(TARGET) $(INSTALL_DIR)/aifilesorter
+
 else ifeq ($(PLATFORM), Windows (64-bit))
-	# Create necessary directories
-	mkdir -p "$(INSTALL_DIR)"/certs
-	mkdir -p "$(INSTALL_DIR)"/gdk-pixbuf-2.0
-	mkdir -p "$(INSTALL_DIR)"/schemas
-	mkdir -p "$(INSTALL_DIR)"/share/icons
-	mkdir -p "$(INSTALL_DIR)"/share/themes
-
-	# Define DLLs and other required files
-	INSTALL_FILES := \
-		$(TARGET) \
-		$(wildcard ./lib/precompiled/*.dll) \
-		C:/msys64/mingw64/bin/gdbus.exe \
-		$(foreach dll, \
-			libcairo-2 libfribidi-0 libharfbuzz-0 libpango-1.0-0 libssl-3-x64 \
-			libcairo-gobject-2 libgcc_s_seh-1 libiconv-2 libpangocairo-1.0-0 \
-			libstdc++-6 libcrypto-3-x64 libgdk-3-0 libidn2-0 libpangoft2-1.0-0 \
-			libthai-0 libtiff-6 libcurl-4 libdatrie-1 libgiomm-2.4-1 libjpeg-8 \
-			libpcre2-8-0 libpangowin32-1.0-0 libunistring-5 libdeflate libepoxy-0 \
-			libglib-2.0-0 libjsoncpp-26 libpng16-16 libwinpthread-1 libLerc libexpat-1 \
-			libglibmm-2.4-1 liblzma-5 libpsl-5 libatk-1.0-0 libffi-8 libgmodule-2.0-0 \
-			libnghttp2-14 libsharpyuv-0 libbrotlicommon libfmt-11 libgobject-2.0-0 \
-			libnghttp3-9 libsigc-2.0-0 zlib1 libbrotlidec libfontconfig-1 libgraphite2 \
-			libngtcp2-16 libsqlite3-0 libbz2-1 libfreetype-6 libgtk-3-0 libngtcp2_crypto_ossl \
-			libssh2-1, \
-			C:/msys64/mingw64/bin/$(dll).dll)
-
-	# Copy executable and DLLs
-	cp -v $(INSTALL_FILES) "$(INSTALL_DIR)/"
-
-	# Copy other required resources
-	cp -v C:/msys64/usr/ssl/cert.pem "$(INSTALL_DIR)/certs/cacert.pem"
-	cp -vr C:/msys64/mingw64/lib/gdk-pixbuf-2.0/* "$(INSTALL_DIR)/gdk-pixbuf-2.0/"
-	cp -vr C:/msys64/mingw64/share/glib-2.0/schemas/* "$(INSTALL_DIR)/schemas/"
-	cp -vr C:/msys64/mingw64/share/icons/* "$(INSTALL_DIR)/share/icons/"
-	cp -vr C:/msys64/mingw64/share/themes/* "$(INSTALL_DIR)/share/themes/"
-
-	# Compile schemas
-	glib-compile-schemas "$(INSTALL_DIR)/schemas"
+	mkdir -p $(INSTALL_DIR)/certs
+	mkdir -p $(INSTALL_DIR)/share
+
+	cp -v $(TARGET) $(INSTALL_DIR)/
+	cp -v ./lib/precompiled/*.dll $(INSTALL_DIR)/
+	cp -v "C:/msys64/mingw64/bin/gdbus.exe" $(INSTALL_DIR)/
+
+	$(foreach dll, \
+		libcairo-2 libfribidi-0 libharfbuzz-0 libpango-1.0-0 libssl-3-x64 \
+		libcairo-gobject-2 libgcc_s_seh-1 libiconv-2 libpangocairo-1.0-0 \
+		libstdc++-6 libcrypto-3-x64 libgdk-3-0 libgdk_pixbuf-2.0-0 libidn2-0 \
+		libpangoft2-1.0-0 libgio-2.0-0 libintl-8 libzstd libjbig-0 libwebp-7 \
+		libthai-0 libtiff-6 libcurl-4 libdatrie-1 libgiomm-2.4-1 libjpeg-8 \
+		libpcre2-8-0 libpangowin32-1.0-0 libunistring-5 libdeflate libepoxy-0 \
+		libglib-2.0-0 libjsoncpp-26 libpng16-16 libwinpthread-1 libLerc libexpat-1 \
+		libglibmm-2.4-1 liblzma-5 libpsl-5 libatk-1.0-0 libffi-8 libgmodule-2.0-0 \
+		libnghttp2-14 libsharpyuv-0 libbrotlicommon libfmt-11 libgobject-2.0-0 \
+		libnghttp3-9 libsigc-2.0-0 zlib1 libbrotlidec libfontconfig-1 libgraphite2 \
+		libngtcp2-16 libsqlite3-0 libbz2-1 libfreetype-6 libgtk-3-0 libngtcp2_crypto_ossl \
+		libssh2-1 libpixman-1-0, \
+		cp -v "C:/msys64/mingw64/bin/$(dll).dll" $(INSTALL_DIR)/;)
+
+	cp -v "C:/msys64/usr/ssl/cert.pem" $(INSTALL_DIR)/certs/cacert.pem
+	cp -vr "C:/msys64/mingw64/lib/gdk-pixbuf-2.0/" $(INSTALL_DIR)/gdk-pixbuf-2.0
+	cp -vr "C:/msys64/mingw64/share/glib-2.0/schemas/" $(INSTALL_DIR)/schemas
+	cp -vr "C:/msys64/mingw64/share/icons/" $(INSTALL_DIR)/share/icons
+	cp -vr "C:/msys64/mingw64/share/themes/" $(INSTALL_DIR)/share/themes
+
+	glib-compile-schemas $(INSTALL_DIR)/schemas
 endif
 
 uninstall:
```

The excerpt is taken from the commit diff for `fix(makefile): for proper  work`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
