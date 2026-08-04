# 2025-11-11: fix(support-dialog): categorization counter

## Covered commits
- `5fdcf56` `2025-11-11` `fix(support-dialog): categorization counter`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/include/Types.hpp`
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/startapp_linux.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/include/MainApp.hpp`, `app/include/Types.hpp`, `app/lib/DatabaseManager.cpp`, `app/lib/MainApp.cpp`, `app/startapp_linux.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(support-dialog): categorization counter`.

Before this commit, the repository reflected the state immediately preceding `5fdcf56`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -206,7 +206,6 @@ private:
     bool suppress_explorer_sync_{false};
     bool suppress_folder_view_sync_{false};
     bool donation_prompt_active_{false};
-    int pending_categorized_count_{0};
     bool should_log_prompts() const;
     void apply_development_logging();
 
diff --git a/app/include/Types.hpp b/app/include/Types.hpp
index 43b9107..37af478 100644
--- a/app/include/Types.hpp
+++ b/app/include/Types.hpp
@@ -19,6 +19,7 @@ struct CategorizedFile {
     std::string category;
     std::string subcategory;
     int taxonomy_id{0};
+    bool from_cache{false};
 };
 
 inline std::string to_string(FileType type) {
```

The excerpt is taken from the commit diff for `fix(support-dialog): categorization counter`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/include/Types.hpp`, `app/lib/DatabaseManager.cpp`, `app/lib/MainApp.cpp`, `app/startapp_linux.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
