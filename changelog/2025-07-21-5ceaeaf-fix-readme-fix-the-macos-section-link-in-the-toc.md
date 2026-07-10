# 2025-07-21: fix(readme): fix the macOS section link in the ToC

## Covered commits
- `5ceaeaf` `2025-07-21` `fix(readme): fix the macOS section link in the ToC`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `5ceaeaf`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -15,7 +15,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 - [Requirements](#requirements)
 - [Installation](#installation)
   - [Windows](#windows)
-  - [MacOS (Apple Silicon)](#macos-apple-silicon)
+  - [MacOS (Apple Silicon)](#macos)
   - [Linux](#linux)
 - [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption)
 - [Uninstallation](#uninstallation)
```

The excerpt is taken from the commit diff for `fix(readme): fix the macOS section link in the ToC`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
