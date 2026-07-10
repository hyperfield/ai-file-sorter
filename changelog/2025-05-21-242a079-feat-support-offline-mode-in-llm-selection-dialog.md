# 2025-05-21: feat: support offline mode in LLM selection dialog

## Covered commits
- `242a079` `2025-05-21` `feat: support offline mode in LLM selection dialog`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/bin/aifilesorter`
- `A` `app/include/DialogUtils.hpp`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `A` `app/lib/DialogUtils.cpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/Utils.cpp`
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/bin/aifilesorter`, `app/include/DialogUtils.hpp`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/lib/DialogUtils.cpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainApp.cpp`, and 3 more file(s). It changed the project from not having the capability described by `feat: support offline mode in LLM selection dialog` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `242a079`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/bin/aifilesorter b/app/bin/aifilesorter
diff --git a/app/include/DialogUtils.hpp b/app/include/DialogUtils.hpp
--- /dev/null
+++ b/app/include/DialogUtils.hpp
@@ -0,0 +1,8 @@
+#include <gtk/gtk.h>
+#include <string>
+
+
+class DialogUtils {
+public:
+    static void show_error_dialog(GtkWindow *parent, const std::string &message);
+};
\ No newline at end of file
diff --git a/app/include/LLMDownloader.hpp b/app/include/LLMDownloader.hpp
index 3b0ba05..a9286ed 100644
--- a/app/include/LLMDownloader.hpp
+++ b/app/include/LLMDownloader.hpp
@@ -13,9 +13,11 @@ class LLMDownloader
 {
 public:
     LLMDownloader();
+    void init_if_needed();
+    bool is_inited();
     void start_download(std::function<void(double)> progress_cb,
                         std::function<void()> on_complete_cb,
-                        std::function<void(const std::string&)> on_status_text);
+                        std::function<void(const std::string &)> on_status_text);
     void try_resume_download();
     bool is_download_resumable() const;
     bool is_download_complete() const;
```

The excerpt is taken from the commit diff for `feat: support offline mode in LLM selection dialog`. The most relevant surfaces are `app/bin/aifilesorter`, `app/include/DialogUtils.hpp`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/lib/DialogUtils.cpp`, and 6 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
