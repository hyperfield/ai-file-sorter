# 2025-05-20: feat(llm-dialog): add file info section and auto-resizing

## Covered commits
- `785ad7f` `2025-05-20` `feat(llm-dialog): add file info section and auto-resizing`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
- Added file info section to Local LLM download dialog
- Implemented automatic dialog resizing when toggling LLM mode
- Improved visibility logic for download-related UI elements

## Files changed
- `M` `app/bin/aifilesorter`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/include/Utils.hpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/Utils.hpp`, `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/Utils.cpp`. It changed the project from not having the capability described by `feat(llm-dialog): add file info section and auto-resizing` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `785ad7f`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/bin/aifilesorter b/app/bin/aifilesorter
diff --git a/app/include/LLMDownloader.hpp b/app/include/LLMDownloader.hpp
--- a/app/include/LLMDownloader.hpp
+++ b/app/include/LLMDownloader.hpp
@@ -24,6 +24,10 @@ public:
                std::function<void(bool, const std::string&)> on_complete);
 
     std::chrono::steady_clock::time_point last_progress_update;
+    std::string url;
+    std::string download_destination;
+    long long real_content_length{0};
+    
     ~LLMDownloader();
 
     enum class DownloadStatus {
@@ -36,9 +40,7 @@ public:
     void cancel_download();
 
 private:
-    std::string url;
     std::string destination_dir;
-    std::string download_destination;
 
     std::thread download_thread;
     std::map<std::string, std::string> curl_headers;
```

The excerpt is taken from the commit diff for `feat(llm-dialog): add file info section and auto-resizing`. The most relevant surfaces are `app/bin/aifilesorter`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/Utils.hpp`, `app/lib/LLMDownloader.cpp`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
