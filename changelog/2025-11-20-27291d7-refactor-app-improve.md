# 2025-11-20: refactor(app): improve

## Covered commits
- `27291d7` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/ConsistencyPassService.hpp`
- `M` `app/include/FileScanner.hpp`
- `M` `app/lib/ConsistencyPassService.cpp`
- `M` `app/lib/FileScanner.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/ConsistencyPassService.hpp`, `app/include/FileScanner.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/FileScanner.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `27291d7`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/ConsistencyPassService.hpp b/app/include/ConsistencyPassService.hpp
--- a/app/include/ConsistencyPassService.hpp
+++ b/app/include/ConsistencyPassService.hpp
@@ -49,6 +49,12 @@ private:
                        std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
                        const ProgressCallback& progress_callback) const;
     void log_chunk_items(const std::vector<const CategorizedFile*>& chunk, const char* stage) const;
+    bool apply_harmonized_response(const std::string& response,
+                                   const std::vector<const CategorizedFile*>& chunk,
+                                   std::unordered_map<std::string, CategorizedFile*>& items_by_key,
+                                   std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
+                                   const ProgressCallback& progress_callback,
+                                   DatabaseManager& db_manager) const;
 
     DatabaseManager& db_manager;
     std::shared_ptr<spdlog::logger> logger;
diff --git a/app/include/FileScanner.hpp b/app/include/FileScanner.hpp
index 80713c5..c4cc984 100644
--- a/app/include/FileScanner.hpp
+++ b/app/include/FileScanner.hpp
@@ -27,9 +27,9 @@ private:
     std::optional<FileType> classify_entry(const fs::directory_entry& entry,
                                            bool bundle,
                                            const ScanContext& context) const;
-    bool is_file_hidden(const fs::path &path);
-    bool is_junk_file(const std::string& name);
-    bool is_file_bundle(const fs::path& path);
+    bool is_file_hidden(const fs::path &path) const;
+    bool is_junk_file(const std::string& name) const;
+    bool is_file_bundle(const fs::path& path) const;
 };
 
 #endif
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/include/ConsistencyPassService.hpp`, `app/include/FileScanner.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/FileScanner.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
