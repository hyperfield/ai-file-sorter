# 2025-11-20: refactor(app): improve

## Covered commits
- `62dc668` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/FileScanner.hpp`
- `M` `app/lib/ConsistencyPassService.cpp`
- `M` `app/lib/FileScanner.cpp`
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/FileScanner.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/FileScanner.cpp`, `app/startapp_windows.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `62dc668`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/FileScanner.hpp b/app/include/FileScanner.hpp
--- a/app/include/FileScanner.hpp
+++ b/app/include/FileScanner.hpp
@@ -20,6 +20,13 @@ private:
     struct ScanContext;
     std::optional<FileEntry> build_entry(const fs::directory_entry& entry,
                                          const ScanContext& context);
+    bool should_skip_entry(const fs::path& entry_path,
+                           const std::string& file_name,
+                           const ScanContext& context,
+                           const std::string& full_path) const;
+    std::optional<FileType> classify_entry(const fs::directory_entry& entry,
+                                           bool bundle,
+                                           const ScanContext& context) const;
     bool is_file_hidden(const fs::path &path);
     bool is_junk_file(const std::string& name);
     bool is_file_bundle(const fs::path& path);
diff --git a/app/lib/ConsistencyPassService.cpp b/app/lib/ConsistencyPassService.cpp
index 4e0ada5..60d8c41 100644
--- a/app/lib/ConsistencyPassService.cpp
+++ b/app/lib/ConsistencyPassService.cpp
@@ -171,6 +171,29 @@ bool parse_structured_lines(
     return true;
 }
 
+const Json::Value* parse_structured_fallback(
+    const std::string& response,
+    Json::Value& root,
+    const std::shared_ptr<spdlog::logger>& logger)
+{
+    return parse_structured_lines(response, root, logger) ? &root : nullptr;
+}
+
+const Json::Value* extract_harmonized_array(Json::Value& root)
+{
+    if (root.isObject() && root.isMember("harmonized")) {
+        const Json::Value& harmonized = root["harmonized"];
+        if (harmonized.isArray()) {
+            return &harmonized;
+        }
+        return nullptr;
+    }
+    if (root.isArray()) {
+        return &root;
+    }
+    return nullptr;
+}
+
 struct HarmonizedUpdate {
     std::string id;
     CategorizedFile* target{nullptr};
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/include/FileScanner.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/FileScanner.cpp`, `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
