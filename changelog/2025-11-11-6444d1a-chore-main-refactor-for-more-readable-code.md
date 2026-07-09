# 2025-11-11: chore(main): refactor for more readable code

## Covered commits
- `6444d1a` `2025-11-11` `chore(main): refactor for more readable code`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/lib/ConsistencyPassService.cpp`
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/MainApp.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/MainApp.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `6444d1a`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -74,6 +74,16 @@ protected:
 
 private:
     void setup_file_explorer();
+    void create_file_explorer_dock();
+    void setup_file_system_model();
+    void setup_file_explorer_view();
+    void connect_file_explorer_signals();
+    void apply_file_explorer_preferences();
+    void restore_tree_settings();
+    void restore_sort_folder_state();
+    void restore_file_scan_options();
+    void restore_file_explorer_visibility();
+    void restore_development_preferences();
     void connect_signals();
     void connect_edit_actions();
     void start_updater();
diff --git a/app/lib/ConsistencyPassService.cpp b/app/lib/ConsistencyPassService.cpp
index 77ef3fb..0530cab 100644
--- a/app/lib/ConsistencyPassService.cpp
+++ b/app/lib/ConsistencyPassService.cpp
@@ -32,6 +32,44 @@ std::string trim_whitespace(const std::string& value) {
     return value.substr(start, end - start + 1);
 }
 
+bool try_parse_harmonized_entry(const std::string& line,
+                                size_t line_number,
+                                const std::string& raw_line,
+                                Json::Value& entry,
+                                const std::shared_ptr<spdlog::logger>& logger)
+{
+    const auto arrow_pos = line.find("=>");
+    if (arrow_pos == std::string::npos) {
+        return false;
+    }
+
+    std::string id = trim_whitespace(line.substr(0, arrow_pos));
+    std::string remainder = trim_whitespace(line.substr(arrow_pos + 2));
+    const auto colon_pos = remainder.find(':');
+    if (colon_pos == std::string::npos) {
+        return false;
+    }
+
+    std::string category = trim_whitespace(remainder.substr(0, colon_pos));
+    std::string subcategory = trim_whitespace(remainder.substr(colon_pos + 1));
+    if (subcategory.empty()) {
+        subcategory = category;
+    }
+
+    if (id.empty() || category.empty()) {
+        if (logger) {
+            logger->warn("Consistency pass skipped malformed line {}: '{}'", line_number, raw_line);
+        }
+        return false;
+    }
+
+    entry = Json::Value(Json::objectValue);
+    entry["id"] = id;
+    entry["category"] = category;
+    entry["subcategory"] = subcategory;
+    return true;
+}
+
 std::string make_item_key(const CategorizedFile& item) {
     std::filesystem::path path(item.file_path);
     path /= item.file_name;
```

The excerpt is taken from the commit diff for `chore(main): refactor for more readable code`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
