# 2025-07-24: fix(filescanner): bug fix, skip junk system files

## Covered commits
- `70834d1` `2025-07-24` `fix(filescanner): bug fix, skip junk system files`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/FileScanner.hpp`
- `M` `app/lib/FileScanner.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/include/FileScanner.hpp`, `app/lib/FileScanner.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(filescanner): bug fix, skip junk system files`.

Before this commit, the repository reflected the state immediately preceding `70834d1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/FileScanner.hpp b/app/include/FileScanner.hpp
--- a/app/include/FileScanner.hpp
+++ b/app/include/FileScanner.hpp
@@ -17,6 +17,7 @@ public:
 
 private:
     bool is_file_hidden(const fs::path &path);
+    bool is_junk_file(const std::string& name);
 };
 
 #endif
\ No newline at end of file
diff --git a/app/lib/FileScanner.cpp b/app/lib/FileScanner.cpp
index 7086e89..a446050 100644
--- a/app/lib/FileScanner.cpp
+++ b/app/lib/FileScanner.cpp
@@ -1,6 +1,7 @@
 #include "FileScanner.hpp"
 #include <iostream>
 #include <filesystem>
+#include <unordered_set>
 #include <gtk/gtk.h>
 
 #ifdef _WIN32
```

The excerpt is taken from the commit diff for `fix(filescanner): bug fix, skip junk system files`. The most relevant surfaces are `app/include/FileScanner.hpp`, `app/lib/FileScanner.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
