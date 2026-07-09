# 2026-02-06: chore(version): udpate:

## Covered commits
- `95a23a5` `2026-02-06` `chore(version): udpate:`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/app_version.hpp`
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `app/include/app_version.hpp`, `app/include/external/llama.cpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `95a23a5`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{1, 6, 0};
+const Version APP_VERSION = Version{1, 6, 1};
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index 1aa7c49..ae9f8df 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 1aa7c497c5e1929771c4fc3fc8e37c7157fa9bbd
+Subproject commit ae9f8df77882716b1702df2bed8919499e64cc28
```

The excerpt is taken from the commit diff for `chore(version): udpate:`. The most relevant surfaces are `app/include/app_version.hpp`, `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
