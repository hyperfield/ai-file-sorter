# 2025-02-06: Fixed the update process

## Covered commits
- `d47633e` `2025-02-06` `Fixed the update process`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/Updater.hpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Updater.cpp`

## What changed from what, why, and how
The commit modified `app/include/Updater.hpp`, `app/lib/MainApp.cpp`, `app/lib/Updater.cpp`. It changed the repository from the prior state to the state described by `Fixed the update process`.

Before this commit, the repository reflected the state immediately preceding `d47633e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/Updater.hpp b/app/include/Updater.hpp
--- a/app/include/Updater.hpp
+++ b/app/include/Updater.hpp
@@ -3,6 +3,7 @@
 
 #include "Settings.hpp"
 #include "Version.hpp"
+#include <future>
 #include <optional>
 #include <string>
 
@@ -37,6 +38,7 @@ private:
     Settings& settings;
     const std::string update_spec_file_url;
     std::optional<UpdateInfo> update_info;
+    std::future<void> update_future;
     void check_updates();
     std::string fetch_update_metadata() const;
     Version string_to_Version(const std::string &version_str);
```

The excerpt is taken from the commit diff for `Fixed the update process`. The most relevant surfaces are `app/include/Updater.hpp`, `app/lib/MainApp.cpp`, `app/lib/Updater.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
