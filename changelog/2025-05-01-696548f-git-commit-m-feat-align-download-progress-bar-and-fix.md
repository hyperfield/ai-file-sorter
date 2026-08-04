# 2025-05-01: git commit -m "feat: align download progress bar and fix crash in download thread"

## Covered commits
- `696548f` `2025-05-01` `git commit -m "feat: align download progress bar and fix crash in download thread"`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`
- `M` `app/bin/aifilesorter`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/include/Utils.hpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/Utils.cpp`
- `M` `app/main.cpp`
- `M` `app/resources/.env`
- `M` `app/resources/resources.c`
- `M` `app/resources/resources.gresource`

## What changed from what, why, and how
The commit modified `app/Makefile`, `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/Utils.hpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/Utils.cpp`, and 4 more file(s). It changed the repository from the prior state to the state described by `git commit -m "feat: align download progress bar and fix crash in download thread"`.

Before this commit, the repository reflected the state immediately preceding `696548f`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -45,6 +45,8 @@ endif
 # Compiler and flags
 CXX = g++
 CXXFLAGS += -std=c++20 -Wall $(shell pkg-config --cflags gtkmm-3.0)
+CXXFLAGS += -g -O0
+
 LDFLAGS += $(shell pkg-config --libs gtkmm-3.0)
 INCLUDE_DIRS = -I./include
 
diff --git a/app/bin/aifilesorter b/app/bin/aifilesorter
index 7277fb9..eab1900 100755
Binary files a/app/bin/aifilesorter and b/app/bin/aifilesorter differ
diff --git a/app/include/LLMDownloader.hpp b/app/include/LLMDownloader.hpp
index c094274..968911a 100644
--- a/app/include/LLMDownloader.hpp
+++ b/app/include/LLMDownloader.hpp
@@ -1,17 +1,49 @@
 #pragma once
-#include <string>
+#include <atomic>
+#include <curl/system.h>
 #include <functional>
+#include <map>
+#include <mutex>
+#include <string>
+#include <thread>
 
 
 class LLMDownloader
 {
 public:
     LLMDownloader();
-    void start_download(std::function<void(double)> progress_callback, std::function<void()> on_complete);
+    void start_download(std::function<void(double)> progress_cb, std::function<void()> on_complete_cb);
     void try_resume_download();
-    bool is_download_resumable();
+    bool is_download_resumable() const;
+
+    void start(std::function<void(double)> on_progress,
+               std::function<void(bool, const std::string&)> on_complete);
+
+    void cancel();
+
+    std::chrono::steady_clock::time_point last_progress_update;
+    ~LLMDownloader();
 
 private:
     std::string url;
-    std::string destination;
+    std::string destination_dir;
+    std::string download_destination;
+
+    std::thread download_thread;
+    std::atomic<bool> stop_requested;
+    std::map<std::string, std::string> curl_headers;
+    mutable std::mutex mutex;
+
+    std::function<void(double)> progress_callback;
+    std::function<void()> on_download_complete;
+    long long real_content_length{0};
+
+    bool resumable = false;
+
+    static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream);
+    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
+    static int progress_func(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);
+
+    std::string get_default_llm_destination();
+    void parse_headers();
 };
\ No newline at end of file
```

The excerpt is taken from the commit diff for `git commit -m "feat: align download progress bar and fix crash in download thread"`. The most relevant surfaces are `app/Makefile`, `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/Utils.hpp`, and 7 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
