# 2025-02-05: Minor fixes. Updated Makefile. Updated the README

## Covered commits
- `ce13d9a` `2025-02-05` `Minor fixes. Updated Makefile. Updated the README`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/Makefile`
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/Makefile`, `app/main.cpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `ce13d9a`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -81,7 +81,7 @@ You can also now launch `Git Bash` from Start Menu.
 4. Install dependencies:
    
 ```bash
-pacman -S --needed mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gdk-pixbuf2 mingw-w64-x86_64-glib2 mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-openssl mingw-w64-x86_64-libx11 mingw-w64-x86_64-libxi mingw-w64-x86_64-libxfixes mingw-w64-x86_64-cairo mingw-w64-x86_64-atk mingw-w64-x86_64-epoxy mingw-w64-x86_64-harfbuzz mingw-w64-x86_64-fontconfig mingw-w64-x86_64-libpng mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libffi mingw-w64-x86_64-pcre mingw-w64-x86_64-libnghttp2 mingw-w64-x86_64-libidn2 mingw-w64-x86_64-librtmp mingw-w64-x86_64-libssh mingw-w64-x86_64-libpsl mingw-w64-x86_64-krb5 mingw-w64-x86_64-openldap mingw-w64-x86_64-brotli mingw-w64-x86_64-libxcb mingw-w64-x86_64-libxrandr mingw-w64-x86_64-libxinerama mingw-w64-x86_64-xkbcommon mingw-w64-x86_64-wayland mingw-w64-x86_64-libthai mingw-w64-x86_64-freetype mingw-w64-x86_64-graphite2 mingw-w64-x86_64-gnutls mingw-w64-x86_64-p11-kit mingw-w64-x86_64-xz mingw-w64-x86_64-lz4 mingw-w64-x86_64-libgcrypt mingw-w64-x86_64-systemd mingw-w64-x86_64-fmt mingw-w64-x86_64-spdlog
+pacman -S --needed mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gdk-pixbuf2 mingw-w64-x86_64-glib2 mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-openssl mingw-w64-x86_64-libx11 mingw-w64-x86_64-libxi mingw-w64-x86_64-libxfixes mingw-w64-x86_64-cairo mingw-w64-x86_64-atk mingw-w64-x86_64-epoxy mingw-w64-x86_64-harfbuzz mingw-w64-x86_64-fontconfig mingw-w64-x86_64-libpng mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libffi mingw-w64-x86_64-pcre mingw-w64-x86_64-libnghttp2 mingw-w64-x86_64-libidn2 mingw-w64-x86_64-librtmp mingw-w64-x86_64-libssh mingw-w64-x86_64-libpsl mingw-w64-x86_64-krb5 mingw-w64-x86_64-openldap mingw-w64-x86_64-brotli mingw-w64-x86_64-libxcb mingw-w64-x86_64-libxrandr mingw-w64-x86_64-libxinerama mingw-w64-x86_64-xkbcommon mingw-w64-x86_64-wayland mingw-w64-x86_64-libthai mingw-w64-x86_64-freetype mingw-w64-x86_64-graphite2 mingw-w64-x86_64-gnutls mingw-w64-x86_64-p11-kit mingw-w64-x86_64-xz mingw-w64-x86_64-lz4 mingw-w64-x86_64-libgcrypt mingw-w64-x86_64-systemd mingw-w64-x86_64-fmt mingw-w64-x86_64-spdlog make
 ```
 5. Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here. The app won't work otherwise.
 
@@ -192,6 +192,8 @@ Before compiling the app:
      ENV_PC=obfuscated-key-part2-value
      ENV_RR=encrypted-data-hex-value
      ```
+
+7. Continue with [Installation](#installation)
 ---
```

The excerpt is taken from the commit diff for `Minor fixes. Updated Makefile. Updated the README`. The most relevant surfaces are `README.md`, `app/Makefile`, `app/main.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
