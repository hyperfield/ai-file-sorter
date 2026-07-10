# 2025-11-06: chore(sanitation): improve

## Covered commits
- `d1edbcf` `2025-11-06` `chore(sanitation): improve`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/CategorizationService.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/CategorizationService.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(sanitation): improve`.

Before this commit, the repository reflected the state immediately preceding `d1edbcf`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/CategorizationService.cpp b/app/lib/CategorizationService.cpp
--- a/app/lib/CategorizationService.cpp
+++ b/app/lib/CategorizationService.cpp
@@ -22,17 +22,20 @@ constexpr const char* kLocalTimeoutEnv = "AI_FILE_SORTER_LOCAL_LLM_TIMEOUT";
 constexpr const char* kRemoteTimeoutEnv = "AI_FILE_SORTER_REMOTE_LLM_TIMEOUT";
 constexpr size_t kMaxConsistencyHints = 5;
 
+std::string trim_whitespace(const std::string& value);
+std::string sanitize_label(const std::string& value);
+
 std::pair<std::string, std::string> split_category_subcategory(const std::string& input) {
     const std::string delimiter = " : ";
 
     const auto pos = input.find(delimiter);
     if (pos == std::string::npos) {
-        return {input, ""};
+        return {sanitize_label(input), ""};
     }
 
     auto category = input.substr(0, pos);
     auto subcategory = input.substr(pos + delimiter.size());
-    return {category, subcategory};
+    return {sanitize_label(category), sanitize_label(subcategory)};
 }
 
 std::string trim_whitespace(const std::string& value) {
@@ -44,6 +47,15 @@ std::string trim_whitespace(const std::string& value) {
     }
     return value.substr(start, end - start + 1);
 }
+
+std::string sanitize_label(const std::string& value) {
+    std::string trimmed = trim_whitespace(value);
+    const auto slash_pos = trimmed.find('/');
+    if (slash_pos != std::string::npos) {
+        trimmed = trimmed.substr(0, slash_pos);
+    }
+    return trim_whitespace(trimmed);
+}
 }
 
 CategorizationService::CategorizationService(Settings& settings,
```

The excerpt is taken from the commit diff for `chore(sanitation): improve`. The most relevant surfaces are `app/lib/CategorizationService.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
