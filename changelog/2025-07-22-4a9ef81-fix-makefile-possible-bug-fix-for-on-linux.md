# 2025-07-22: fix(makefile): possible bug fix for  on Linux

## Covered commits
- `4a9ef81` `2025-07-22` `fix(makefile): possible bug fix for  on Linux`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(makefile): possible bug fix for  on Linux`.

Before this commit, the repository reflected the state immediately preceding `4a9ef81`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -16,6 +16,7 @@ ifeq ($(UNAME), Linux)
 
 	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
 	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
+	LDFLAGS += -Wl,-rpath-link=./lib/precompiled
 
 
 else ifeq ($(UNAME), Darwin)
```

The excerpt is taken from the commit diff for `fix(makefile): possible bug fix for  on Linux`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
