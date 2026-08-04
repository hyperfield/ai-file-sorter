# 2025-02-06: Updated the README

## Covered commits
- `2b497c5` `2025-02-06` `Updated the README`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `2b497c5`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -165,8 +165,9 @@ Before compiling the app:
 2. Generate a 32-character random secret key, e.g., using [this tool](https://passwords-generator.org/32-character).
 
     **Important**: If you're compiling on Windows, make sure there is NO `=` in the generated key! If one or more `=` are there, regenerate the key!
+    **Important**: If you're compiling on Windows, it's probably best to avoid symbols due to possible unpredictable parsing issues.
 
-    Your secret key could look something like `du)]--Wg#+Au89Ro6eRMJc"]qx~owL_X`.
+    Your secret key could look something like `sVPV2fWoRg5q62AuCGVQ4p0NbHIU5DEv` or `du)]--Wg#+Au89Ro6eRMJc"]qx~owL_X`.
 
 3. Navigate to the `api-key-encryption` folder, then make a file named `encryption.ini` with the following content:
 
@@ -176,7 +177,7 @@ Before compiling the app:
     ```
 
 4. Run the `compile.sh` script in the same directory to generate the executable `obfuscate_encrypt`.
-
+ due 
 5. Execute `obfuscate_encrypt` to generate:
    - Obfuscated Key part 1
    - Obfuscated Key part 2
```

The excerpt is taken from the commit diff for `Updated the README`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
