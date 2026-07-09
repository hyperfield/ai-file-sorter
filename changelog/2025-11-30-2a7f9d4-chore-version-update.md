# 2025-11-30: chore(version): update

## Covered commits
- `2a7f9d4` `2025-11-30` `chore(version): update`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `CHANGELOG.md`
- `M` `README.md`
- `M` `app/CMakeLists.txt`
- `M` `app/build_windows.ps1`

## What changed from what, why, and how
The commit adjusted version-bearing files in `CHANGELOG.md`, `README.md`, `app/CMakeLists.txt`, `app/build_windows.ps1`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `2a7f9d4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,11 @@
 # Changelog
 
+## [1.4.0] - 2025-12-30
+- Added dry run / preview-only mode with From→To table, no moves performed until you uncheck.
+- Persistent Undo: the latest sort saves a plan file; use Edit → “Undo last run” even after closing dialogs.
+- UI tweaks: Name column auto-resizes, new translations for dry run/undo strings, Undo moved to top of Edit menu.
+- A few more guard rails added.
+
 ## [1.3.0] - 2025-11-21
 
 - You can now switch between two categorization modes: More Refined and More Consistent. Choose depending on your folder and use case.
diff --git a/README.md b/README.md
index 5ece594..e5d7d4d 100644
--- a/README.md
+++ b/README.md
@@ -74,7 +74,13 @@ File content–based sorting for certain file types is also in development.
 
 ## Changelog
 
-See [CHANGELOG.md](CHANGELOG.md) for the release history.
+## [1.4.0] - 2025-12-30
+- Added dry run / preview-only mode with From→To table, no moves performed until you uncheck.
+- Persistent Undo: the latest sort saves a plan file; use Edit → “Undo last run” even after closing dialogs.
+- UI tweaks: Name column auto-resizes, new translations for dry run/undo strings, Undo moved to top of Edit menu.
+- A few more guard rails added.
+
+See [CHANGELOG.md](CHANGELOG.md) for the full history.
 
 ---
```

The excerpt is taken from the commit diff for `chore(version): update`. The most relevant surfaces are `CHANGELOG.md`, `README.md`, `app/CMakeLists.txt`, `app/build_windows.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
