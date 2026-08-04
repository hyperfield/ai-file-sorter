# 2025-11-20: refactor(app): improve

## Covered commits
- `09c1ac9` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/ConsistencyPassService.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/ConsistencyPassService.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `09c1ac9`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/ConsistencyPassService.cpp b/app/lib/ConsistencyPassService.cpp
--- a/app/lib/ConsistencyPassService.cpp
+++ b/app/lib/ConsistencyPassService.cpp
@@ -353,6 +353,39 @@ std::pair<std::string, std::string> split_category_subcategory(const std::string
     return {lhs, std::string()};
 }
 
+std::optional<std::pair<std::string, std::string>> parse_ordered_line(
+    std::string line,
+    const std::string& raw_line,
+    size_t line_number,
+    const std::shared_ptr<spdlog::logger>& logger)
+{
+    line = strip_list_prefix(std::move(line));
+    const auto key_value = split_key_value(line);
+    if (!key_value.has_value()) {
+        return std::nullopt;
+    }
+
+    const auto& [lhs, rhs_raw] = *key_value;
+    auto [category, subcategory] = split_category_subcategory(lhs);
+    std::string rhs = rhs_raw;
+
+    if (subcategory.empty()) {
+        subcategory = rhs;
+    }
+    if (subcategory.empty()) {
+        subcategory = category;
+    }
+
+    if (category.empty()) {
+        if (logger) {
+            logger->warn("Consistency pass fallback skipped malformed line {}: '{}'", line_number, raw_line);
+        }
+        return std::nullopt;
+    }
+
+    return std::make_pair(std::move(category), std::move(subcategory));
+}
+
 std::vector<std::pair<std::string, std::string>> parse_ordered_category_lines(
     const std::string& response,
     const std::shared_ptr<spdlog::logger>& logger)
@@ -371,31 +404,9 @@ std::vector<std::pair<std::string, std::string>> parse_ordered_category_lines(
         if (line == "END") {
             break;
         }
-        line = strip_list_prefix(std::move(line));
-        const auto key_value = split_key_value(line);
-        if (!key_value.has_value()) {
-            continue;
-        }
-
-        const auto& [lhs, rhs_raw] = *key_value;
-        auto [category, subcategory] = split_category_subcategory(lhs);
-        std::string rhs = rhs_raw;
-
-        if (subcategory.empty()) {
-            subcategory = rhs;
+        if (auto parsed = parse_ordered_line(line, raw_line, line_number, logger)) {
+            ordered.push_back(std::move(*parsed));
         }
-        if (subcategory.empty()) {
-            subcategory = category;
-        }
-
-        if (category.empty()) {
-            if (logger) {
-                logger->warn("Consistency pass fallback skipped malformed line {}: '{}'", line_number, raw_line);
-            }
-            continue;
-        }
-
-        ordered.emplace_back(std::move(category), std::move(subcategory));
     }
 
     if (ordered.empty() && logger) {
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/lib/ConsistencyPassService.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
