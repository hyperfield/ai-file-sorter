# 2025-04-24: Work in progress: LLM selection and download dialog and its functionality

## Covered commits
- `147e5ed` `2025-04-24` `Work in progress: LLM selection and download dialog and its functionality`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `app/bin/aifilesorter`
- `A` `app/include/LLMDownloader.hpp`
- `A` `app/include/LLMSelectionDialog.hpp`
- `A` `app/lib/LLMDownloader.cpp`
- `A` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/Updater.cpp`
- `M` `app/main.cpp`
- `M` `app/resources/.env`
- `M` `app/resources/resources.c`
- `M` `app/resources/resources.gresource`

## What changed from what, why, and how
The commit modified `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/Updater.cpp`, `app/main.cpp`, `app/resources/.env`, and 2 more file(s). It changed the repository from the prior state to the state described by `Work in progress: LLM selection and download dialog and its functionality`.

Before this commit, the repository reflected the state immediately preceding `147e5ed`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/bin/aifilesorter b/app/bin/aifilesorter
diff --git a/app/include/LLMDownloader.hpp b/app/include/LLMDownloader.hpp
--- /dev/null
+++ b/app/include/LLMDownloader.hpp
@@ -0,0 +1,17 @@
+#pragma once
+#include <string>
+#include <functional>
+
+
+class LLMDownloader
+{
+public:
+    LLMDownloader();
+    void start_download(std::function<void(double)> progress_callback, std::function<void()> on_complete);
+    void try_resume_download();
+    bool is_download_resumable();
+
+private:
+    std::string url;
+    std::string destination;
+};
\ No newline at end of file
diff --git a/app/include/LLMSelectionDialog.hpp b/app/include/LLMSelectionDialog.hpp
new file mode 100644
index 0000000..77416fc
--- /dev/null
+++ b/app/include/LLMSelectionDialog.hpp
@@ -0,0 +1,34 @@
+#pragma once
+#include <gtk/gtk.h>
+#include <atomic>
+#include <mutex>
+#include <thread>
+
+class LLMSelectionDialog {
+public:
+    static void on_llm_radio_toggled(GtkWidget *widget, gpointer data);
+    LLMSelectionDialog();
+    ~LLMSelectionDialog();
+    int run();
+    GtkWidget* get_widget(); 
+
+private:
+    GtkWidget *dialog;
+    GtkWidget *main_box;
+    GtkWidget *title_label;
+    GtkWidget *remote_llm_button;
+    GtkWidget *local_llm_button;
+    GtkWidget *download_button;
+    GtkWidget *progress_bar;
+    GtkWidget *continue_button;
+
+    std::unique_ptr<LLMDownloader> downloader;
+    std::thread download_thread;
+    std::atomic<bool> is_downloading{false};
+    std::mutex download_mutex;
+
+    void on_selection_changed(GtkWidget *widget, gpointer data);
+    void on_download_button_clicked(GtkWidget *widget, gpointer data);
+    void update_progress();
+    void on_download_complete();
+};
\ No newline at end of file
```

The excerpt is taken from the commit diff for `Work in progress: LLM selection and download dialog and its functionality`. The most relevant surfaces are `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, and 5 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
