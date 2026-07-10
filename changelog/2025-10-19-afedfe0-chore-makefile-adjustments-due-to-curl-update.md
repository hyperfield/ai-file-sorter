# 2025-10-19: chore(makefile): adjustments due to curl update

## Covered commits
- `afedfe0` `2025-10-19` `chore(makefile): adjustments due to curl update`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/Makefile`. It changed the repository support state, metadata, or supporting files in the way described by `chore(makefile): adjustments due to curl update`.

Before this commit, the repository reflected the state immediately preceding `afedfe0`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -149,7 +149,7 @@ else ifeq ($(PLATFORM), Windows (64-bit))
 		libgmodule-2.0-0 libnghttp2-14 libsharpyuv-0 libbrotlicommon libfmt-11 \
 		libgobject-2.0-0 libnghttp3-9 libsigc-2.0-0 zlib1 libbrotlidec libfontconfig-1 \
 		libgraphite2 libopenblas libngtcp2-16 libsqlite3-0 libbz2-1 libfreetype-6 \
-		libgtk-3-0 libngtcp2_crypto_ossl libquadmath-0 libssh2-1 libpixman-1-0, \
+		libgtk-3-0 libngtcp2_crypto_ossl-0 libquadmath-0 libssh2-1 libpixman-1-0, \
 		cp -v "C:/msys64/mingw64/bin/$(dll).dll" $(INSTALL_DIR)/;)
 
 	cp -v "C:/msys64/usr/ssl/cert.pem" $(INSTALL_DIR)/certs/cacert.pem
```

The excerpt is taken from the commit diff for `chore(makefile): adjustments due to curl update`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
