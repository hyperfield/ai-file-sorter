# 2025-07-25: fix(filescanner): fix missing inclusion for compile on Windows

## Covered commits
- `fac7fb4` `2025-07-25` `fix(filescanner): fix missing inclusion for compile on Windows`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/FileScanner.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/FileScanner.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(filescanner): fix missing inclusion for compile on Windows`.

Before this commit, the repository reflected the state immediately preceding `fac7fb4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/FileScanner.cpp b/app/lib/FileScanner.cpp
--- a/app/lib/FileScanner.cpp
+++ b/app/lib/FileScanner.cpp
@@ -1,4 +1,5 @@
 #include "FileScanner.hpp"
+#include <algorithm>
 #include <iostream>
 #include <filesystem>
 #include <unordered_set>
```

The excerpt is taken from the commit diff for `fix(filescanner): fix missing inclusion for compile on Windows`. The most relevant surfaces are `app/lib/FileScanner.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
