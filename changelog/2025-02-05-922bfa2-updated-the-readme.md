# 2025-02-05: Updated the README

## Covered commits
- `922bfa2` `2025-02-05` `Updated the README`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `922bfa2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -87,7 +87,7 @@ pacman -S --needed mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gdk-pixbuf2 mingw-w64-
 
 6. Go to `app/resources` and run `./compile-resources.sh`. Go back to the `app` directory.
 
-7. Run `make`, `make install` and `make clean`. The executable `aifilesorter.exe` will be located in `C:\Program Files\AIFileSorter`. You can add the directory to `%PATH%`.
+7. Run `make`, `make install` and `make clean`. The executable `AiFileSorter.exe` will be located in `C:\Program Files\AiFileSorter`. You can add the directory to `%PATH%`.
 
 ---
```

The excerpt is taken from the commit diff for `Updated the README`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
