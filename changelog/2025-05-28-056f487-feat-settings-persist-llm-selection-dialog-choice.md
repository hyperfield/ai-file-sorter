# 2025-05-28: feat(settings): persist LLM selection dialog choice

## Covered commits
- `056f487` `2025-05-28` `feat(settings): persist LLM selection dialog choice`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/bin/aifilesorter`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/Settings.hpp`
- `M` `app/include/Types.hpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/MainApp.hpp`, `app/include/Settings.hpp`, `app/include/Types.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainApp.cpp`, and 2 more file(s). It changed the project from not having the capability described by `feat(settings): persist LLM selection dialog choice` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `056f487`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/bin/aifilesorter b/app/bin/aifilesorter
diff --git a/app/include/LLMDownloader.hpp b/app/include/LLMDownloader.hpp
--- a/app/include/LLMDownloader.hpp
+++ b/app/include/LLMDownloader.hpp
@@ -1,4 +1,5 @@
 #pragma once
+#include "Settings.hpp"
 #include <atomic>
 #include <curl/system.h>
 #include <functional>
diff --git a/app/include/LLMSelectionDialog.hpp b/app/include/LLMSelectionDialog.hpp
index 387624b..659ceaf 100644
--- a/app/include/LLMSelectionDialog.hpp
+++ b/app/include/LLMSelectionDialog.hpp
@@ -1,15 +1,18 @@
 #pragma once
+#include "LLMDownloader.hpp"
+#include "Types.hpp"
 #include <gtk/gtk.h>
 #include <atomic>
 #include <mutex>
 #include <thread>
-#include "LLMDownloader.hpp"
+
 
 class LLMSelectionDialog {
 public:
     static void on_llm_radio_toggled(GtkWidget *widget, gpointer data);
-    LLMSelectionDialog();
+    LLMSelectionDialog(Settings& settings);
     ~LLMSelectionDialog();
+    LLMChoice get_selected_llm_choice() const;
     int run();
     GtkWidget* get_widget();
```

The excerpt is taken from the commit diff for `feat(settings): persist LLM selection dialog choice`. The most relevant surfaces are `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/MainApp.hpp`, `app/include/Settings.hpp`, and 5 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
