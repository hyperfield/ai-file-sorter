# 2025-10-19: feat(logging): improve logging coverage

## Covered commits
- `ebfe3fc` `2025-10-19` `feat(logging): improve logging coverage`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/CategorizationProgressDialog.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MovableCategorizedFile.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/lib/CategorizationProgressDialog.cpp`, `app/lib/MainApp.cpp`, `app/lib/MovableCategorizedFile.cpp`. It changed the project from not having the capability described by `feat(logging): improve logging coverage` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `ebfe3fc`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/CategorizationProgressDialog.cpp b/app/lib/CategorizationProgressDialog.cpp
--- a/app/lib/CategorizationProgressDialog.cpp
+++ b/app/lib/CategorizationProgressDialog.cpp
@@ -1,5 +1,6 @@
 #include "CategorizationProgressDialog.hpp"
 #include "MainApp.hpp"
+#include "Logger.hpp"
 #include <gtk/gtk.h>
 #include <gtk/gtktypes.h>
 #include <gobject/gsignal.h>
@@ -105,7 +106,9 @@ void CategorizationProgressDialog::show()
 void CategorizationProgressDialog::append_text(const std::string& text)
 {
     if (!m_TextView) {
-        g_printerr("Error: text_view is not initialized!\n");
+        if (auto logger = Logger::get_logger("core_logger")) {
+            logger->error("CategorizationProgressDialog text view not initialized");
+        }
         return;
     }
```

The excerpt is taken from the commit diff for `feat(logging): improve logging coverage`. The most relevant surfaces are `app/lib/CategorizationProgressDialog.cpp`, `app/lib/MainApp.cpp`, `app/lib/MovableCategorizedFile.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
