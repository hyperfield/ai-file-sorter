# 2025-08-01: chore(app-version): Change app version to 0.9.1

## Covered commits
- `ec10f56` `2025-08-01` `chore(app-version): Change app version to 0.9.1`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/app_version.hpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `app/include/app_version.hpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `ec10f56`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{0, 9, 0};
\ No newline at end of file
+const Version APP_VERSION = Version{0, 9, 1};
\ No newline at end of file
```

The excerpt is taken from the commit diff for `chore(app-version): Change app version to 0.9.1`. The most relevant surfaces are `app/include/app_version.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
