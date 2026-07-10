# 2025-12-04: chore(readme): update

## Covered commits
- `a971329` `2025-12-04` `chore(readme): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `A` `app/resources/images/icon_1080x1080.png`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `app/resources/images/icon_1080x1080.png`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `a971329`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -39,7 +39,7 @@ File content–based sorting for certain file types is also in development.
 1. Point it at a folder or drive  
 2. It runs a local LLM to analyze your files  
 3. The LLM suggests categorizations  
-4. You review and adjust if needed — done  
+4. You review and adjust if needed - done  
 
 ---
 
diff --git a/app/resources/images/icon_1080x1080.png b/app/resources/images/icon_1080x1080.png
new file mode 100644
index 0000000..ec5a156
Binary files /dev/null and b/app/resources/images/icon_1080x1080.png differ
```

The excerpt is taken from the commit diff for `chore(readme): update`. The most relevant surfaces are `README.md`, `app/resources/images/icon_1080x1080.png`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
