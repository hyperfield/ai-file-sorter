# 2025-11-24: chore(version): update (for Windows binary purposes)

## Covered commits
- `d47a899` `2025-11-24` `chore(version): update (for Windows binary purposes)`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/app_version.hpp`
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `app/include/app_version.hpp`, `app/include/external/llama.cpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `d47a899`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{1, 3, 0};
+const Version APP_VERSION = Version{1, 3, 2};
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index ee09828..7675c55 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ee09828cb057460b369576410601a3a09279e23c
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
```

The excerpt is taken from the commit diff for `chore(version): update (for Windows binary purposes)`. The most relevant surfaces are `app/include/app_version.hpp`, `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
