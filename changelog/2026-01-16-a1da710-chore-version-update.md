# 2026-01-16: chore(version): update

## Covered commits
- `a1da710` `2026-01-16` `chore(version): update`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `app/include/app_version.hpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `CHANGELOG.md`, `app/include/app_version.hpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `a1da710`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,11 @@
 ﻿# Changelog
 
+## [1.6.0] - 2026-01-16
+
+- Local 3B model download now defaults to Q4 for better GPU compatibility.
+- Legacy Local 3B Q8 is still selectable when an existing download is found.
+- LLM selection dialog now uses local file sizes for completed downloads when remote size metadata is unavailable.
+
 ## [1.5.0] - 2026-01-11
 
 - Added content analysis for picture files via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
index 46c5d70..12df983 100644
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{1, 5, 0};
+const Version APP_VERSION = Version{1, 6, 0};
```

The excerpt is taken from the commit diff for `chore(version): update`. The most relevant surfaces are `CHANGELOG.md`, `app/include/app_version.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
