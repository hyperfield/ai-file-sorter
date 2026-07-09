# 2025-10-30: chore(readme): update

## Covered commits
- `b3fba99` `2025-10-30` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `A` `screenshots/categorization-dialog-macos.png`
- `A` `screenshots/main_windows_macos.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `screenshots/categorization-dialog-macos.png`, `screenshots/main_windows_macos.png`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `b3fba99`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -7,7 +7,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 [![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 
-![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-1.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-2.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-3.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-4.png)
+![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-1.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-2.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-3.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-4.png) ![AI File Sorter Screenshot](screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](screenshots/categorization-dialog-macos.png)
 
 ---
 
@@ -295,7 +295,7 @@ Before compiling the app:
     SECRET_KEY=your-generated-32-byte-secret-key
     ```
 
-4. Run the `compile.sh` script in the same directory to generate the executable `obfuscate_encrypt`.
+4. Run the `compile.sh` (or `compile_mac.sh`) script in the same directory to generate the executable `obfuscate_encrypt`.
  due 
 5. Execute `obfuscate_encrypt` to generate:
    - Obfuscated Key part 1
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`, `screenshots/categorization-dialog-macos.png`, `screenshots/main_windows_macos.png`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
