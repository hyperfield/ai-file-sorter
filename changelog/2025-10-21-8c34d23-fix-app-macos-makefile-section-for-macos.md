# 2025-10-21: fix(app-macos): Makefile section for macOS

## Covered commits
- `8c34d23` `2025-10-21` `fix(app-macos): Makefile section for macOS`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.gitignore`
- `M` `app/Makefile`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit corrected behavior in `.gitignore`, `app/Makefile`, `app/include/external/llama.cpp`, `app/lib/LocalLLMClient.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(app-macos): Makefile section for macOS`.

Before this commit, the repository reflected the state immediately preceding `8c34d23`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -70,6 +70,9 @@ vcpkg_installed/
 # Local History for Visual Studio Code
 .history/
 
+# Cache directories
+.cache/
+
 # Built Visual Studio Code Extensions
 *.vsix
 
diff --git a/app/Makefile b/app/Makefile
index fd73e96..13925ba 100644
--- a/app/Makefile
+++ b/app/Makefile
@@ -25,9 +25,10 @@ ifeq ($(UNAME), Linux)
 	LDFLAGS += -Wl,-rpath-link=./lib/precompiled
 
 else ifeq ($(UNAME), Darwin)
-    export PATH := /opt/homebrew/opt/curl/bin:$(PATH)
+    PATH := /opt/homebrew/opt/qt/bin:/opt/homebrew/opt/curl/bin:$(PATH)
+    export PATH
 
-    export PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:/opt/homebrew/opt/libffi/lib/pkgconfig:/opt/homebrew/opt/expat/lib/pkgconfig:$(PKG_CONFIG_PATH)
+    export PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:/opt/homebrew/opt/libffi/lib/pkgconfig:/opt/homebrew/opt/expat/lib/pkgconfig:/opt/homebrew/opt/qt/lib/pkgconfig:$(PKG_CONFIG_PATH)
 
     export LDFLAGS += -L/opt/homebrew/opt/libffi/lib
     export CPPFLAGS += -I/opt/homebrew/opt/libffi/include
```

The excerpt is taken from the commit diff for `fix(app-macos): Makefile section for macOS`. The most relevant surfaces are `.gitignore`, `app/Makefile`, `app/include/external/llama.cpp`, `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
