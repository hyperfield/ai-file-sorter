# 2025-11-08: feat(ui): point file tree explorer to 'Folder' field location

## Covered commits
- `7e9d436` `2025-11-08` `feat(ui): point file tree explorer to 'Folder' field location`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/include/MainApp.hpp`, `app/lib/MainApp.cpp`. It changed the project from not having the capability described by `feat(ui): point file tree explorer to 'Folder' field location` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `7e9d436`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -97,6 +97,7 @@ private:
     void update_analyze_button_state(bool analyzing);
     void update_results_view_mode();
     void update_folder_contents(const QString& directory);
+    void focus_file_explorer_on_path(const QString& path);
 
     void handle_analysis_finished();
     void handle_analysis_failure(const std::string& message);
@@ -194,6 +195,8 @@ private:
     std::atomic<bool> stop_analysis{false};
     bool analysis_in_progress_{false};
     bool status_is_ready_{true};
+    bool suppress_explorer_sync_{false};
+    bool suppress_folder_view_sync_{false};
     bool should_log_prompts() const;
     void apply_development_logging();
 };
```

The excerpt is taken from the commit diff for `feat(ui): point file tree explorer to 'Folder' field location`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
