# 2025-07-25: chore(makefile): return the -mwindows flag

## Covered commits
- `0855787` `2025-07-25` `chore(makefile): return the -mwindows flag`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/Makefile`. It changed the repository support state, metadata, or supporting files in the way described by `chore(makefile): return the -mwindows flag`.

Before this commit, the repository reflected the state immediately preceding `0855787`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -51,7 +51,7 @@ else ifeq ($(UNAME), MINGW64_NT)
     RC_FILE = resources/exe_icon.rc
     RC_OBJ = resources/exe_icon.o
 
-    LDFLAGS += -lwininet -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
+    LDFLAGS += -mwindows -lwininet -ljsoncpp -lcrypto -lcurl -lspdlog -lsqlite3 -lssl -lfmt -lllama -lggml
 endif
 
 # Compiler and flags
```

The excerpt is taken from the commit diff for `chore(makefile): return the -mwindows flag`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
