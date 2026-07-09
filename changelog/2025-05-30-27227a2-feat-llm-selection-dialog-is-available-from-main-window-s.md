# 2025-05-30: feat: LLM selection dialog is available from main window's Settings menu item

## Covered commits
- `27227a2` `2025-05-30` `feat: LLM selection dialog is available from main window's Settings menu item`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
feat: download progress bar now shows current progress even before resuming download

fix: minor adjustments and cleanup

## Files changed
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/main.cpp`
- `M` `app/resources/resources.c`
- `M` `app/resources/resources.gresource`
- `A` `app/resources/ui/#main_window.glade#`
- `M` `app/resources/ui/main_window.glade`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/MainApp.hpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainApp.cpp`, `app/main.cpp`, `app/resources/resources.c`, and 3 more file(s). It changed the project from not having the capability described by `feat: LLM selection dialog is available from main window's Settings menu item` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `27227a2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/LLMDownloader.hpp b/app/include/LLMDownloader.hpp
--- a/app/include/LLMDownloader.hpp
+++ b/app/include/LLMDownloader.hpp
@@ -22,14 +22,15 @@ public:
     void try_resume_download();
     bool is_download_resumable() const;
     bool is_download_complete() const;
-    
+
+    long long get_real_content_length() const;
+    std::string get_download_destination() const;
+
     void start(std::function<void(double)> on_progress,
                std::function<void(bool, const std::string&)> on_complete);
 
     std::chrono::steady_clock::time_point last_progress_update;
     std::string url;
-    std::string download_destination;
-    long long real_content_length{0};
     
     ~LLMDownloader();
 
@@ -54,7 +55,9 @@ private:
     std::function<void()> on_download_complete;
     std::function<void(const std::string&)> on_status_text;
 
-    bool resumable = false;
+    bool resumable{false};
+    long long real_content_length{0};
+    std::string download_destination;
 
     static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream);
     static size_t discard_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
```

The excerpt is taken from the commit diff for `feat: LLM selection dialog is available from main window's Settings menu item`. The most relevant surfaces are `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/MainApp.hpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, and 6 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
