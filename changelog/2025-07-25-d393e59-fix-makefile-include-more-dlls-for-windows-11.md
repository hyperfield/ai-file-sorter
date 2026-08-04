# 2025-07-25: fix(makefile): include more dlls for Windows 11

## Covered commits
- `d393e59` `2025-07-25` `fix(makefile): include more dlls for Windows 11`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(makefile): include more dlls for Windows 11`.

Before this commit, the repository reflected the state immediately preceding `d393e59`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -51,7 +51,7 @@ else ifeq ($(UNAME), MINGW64_NT)
     RC_FILE = resources/exe_icon.rc
     RC_OBJ = resources/exe_icon.o
 
-    LDFLAGS += -mwindows -lwininet -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
+    LDFLAGS += -lwininet -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
 endif
 
 # Compiler and flags
@@ -140,16 +140,16 @@ else ifeq ($(PLATFORM), Windows (64-bit))
 	$(foreach dll, \
 		libcairo-2 libfribidi-0 libharfbuzz-0 libpango-1.0-0 libssl-3-x64 \
 		libcairo-gobject-2 libgcc_s_seh-1 libiconv-2 libpangocairo-1.0-0 \
-		libstdc++-6 libcrypto-3-x64 libgdk-3-0 libgdk_pixbuf-2.0-0 libidn2-0 \
-		libpangoft2-1.0-0 libgio-2.0-0 libintl-8 libzstd libjbig-0 libwebp-7 \
-		libthai-0 libtiff-6 libcurl-4 libdatrie-1 libgiomm-2.4-1 libjpeg-8 \
-		libpcre2-8-0 libpangowin32-1.0-0 libunistring-5 libdeflate libepoxy-0 \
-		libglib-2.0-0 libjsoncpp-26 libpng16-16 libwinpthread-1 libLerc libexpat-1 \
-		libglibmm-2.4-1 liblzma-5 libpsl-5 libatk-1.0-0 libffi-8 libgmodule-2.0-0 \
-		libnghttp2-14 libsharpyuv-0 libbrotlicommon libfmt-11 libgobject-2.0-0 \
-		libnghttp3-9 libsigc-2.0-0 zlib1 libbrotlidec libfontconfig-1 libgraphite2 \
-		libngtcp2-16 libsqlite3-0 libbz2-1 libfreetype-6 libgtk-3-0 libngtcp2_crypto_ossl \
-		libssh2-1 libpixman-1-0, \
+		libstdc++-6 libcrypto-3-x64 libgdk-3-0 libgdk_pixbuf-2.0-0 libgfortran-5 \
+		libidn2-0 libpangoft2-1.0-0 libgio-2.0-0 libintl-8 libgomp-1 libzstd \
+		libjbig-0 libwebp-7 libthai-0 libtiff-6 libcurl-4 libdatrie-1 \
+		libgiomm-2.4-1 libjpeg-8 libpcre2-8-0 libpangowin32-1.0-0 libunistring-5 \
+		libdeflate libepoxy-0 libglib-2.0-0 libjsoncpp-26 libpng16-16 libwinpthread-1 \
+		libLerc libexpat-1 libglibmm-2.4-1 liblzma-5 libpsl-5 libatk-1.0-0 libffi-8 \
+		libgmodule-2.0-0 libnghttp2-14 libsharpyuv-0 libbrotlicommon libfmt-11 \
+		libgobject-2.0-0 libnghttp3-9 libsigc-2.0-0 zlib1 libbrotlidec libfontconfig-1 \
+		libgraphite2 libopenblas libngtcp2-16 libsqlite3-0 libbz2-1 libfreetype-6 \
+		libgtk-3-0 libngtcp2_crypto_ossl libquadmath-0 libssh2-1 libpixman-1-0, \
 		cp -v "C:/msys64/mingw64/bin/$(dll).dll" $(INSTALL_DIR)/;)
 
 	cp -v "C:/msys64/usr/ssl/cert.pem" $(INSTALL_DIR)/certs/cacert.pem
```

The excerpt is taken from the commit diff for `fix(makefile): include more dlls for Windows 11`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
