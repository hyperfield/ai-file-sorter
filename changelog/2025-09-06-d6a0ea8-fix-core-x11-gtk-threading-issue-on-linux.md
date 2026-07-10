# 2025-09-06: fix(core): X11 / Gtk threading issue on Linux

## Covered commits
- `d6a0ea8` `2025-09-06` `fix(core): X11 / Gtk threading issue on Linux`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`
- `M` `app/include/external/llama.cpp`
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`, `app/include/external/llama.cpp`, `app/main.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(core): X11 / Gtk threading issue on Linux`.

Before this commit, the repository reflected the state immediately preceding `d6a0ea8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -14,7 +14,7 @@ ifeq ($(UNAME), Linux)
     INSTALL_LIB_DIR := /usr/local/lib/aifilesorter
 	LD_CONF_FILE := /etc/ld.so.conf.d/aifilesorter.conf
 
-	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
+	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lX11 -pthread
 	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
 	LDFLAGS += -Wl,-rpath-link=./lib/precompiled
 
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index 7675c55..8e6f8bc 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
+Subproject commit 8e6f8bc875358968b63e08f7bbbe0a288f29d856
```

The excerpt is taken from the commit diff for `fix(core): X11 / Gtk threading issue on Linux`. The most relevant surfaces are `app/Makefile`, `app/include/external/llama.cpp`, `app/main.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
