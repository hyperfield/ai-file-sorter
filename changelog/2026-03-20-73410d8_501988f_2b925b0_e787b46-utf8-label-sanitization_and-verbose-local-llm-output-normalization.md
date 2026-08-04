# 2026-03-20: UTF-8 label sanitization and normalization of verbose local-LLM output

## Covered commits
- `73410d8` `2026-03-20` `fix(encoding): sanitize malformed utf-8 labels from cache`
- `501988f` `2026-03-20` `test(encoding): cover malformed utf-8 label sanitization`
- `2b925b0` `2026-03-20` `fix(local-llm): preserve prompt budget and sanitize verbose replies`
- `e787b46` `2026-03-20` `fix(categorization): normalize verbose llm label output`

## Motivation
Once categorization results were cached aggressively, malformed UTF-8 labels could poison later runs. Local models were also returning increasingly verbose labels that needed prompt-budget and sanitization pressure to remain parseable. The fixes belong together because both were about hardening the text boundary between the model and the cache/UI.

## What changed
These commits sanitized malformed UTF-8 category labels in cache interactions, added tests for the encoding path, preserved prompt-budget constraints against verbose model replies, and normalized overly verbose label output before it reached the user-facing taxonomy.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `73410d8`
```diff
diff --git a/app/lib/DatabaseManager.cpp b/app/lib/DatabaseManager.cpp
--- a/app/lib/DatabaseManager.cpp
+++ b/app/lib/DatabaseManager.cpp
@@ -1,6 +1,7 @@
 #include "DatabaseManager.hpp"
 #include "Types.hpp"
 #include "Logger.hpp"
+#include "Utils.hpp"
 
 #include <algorithm>
 #include <cctype>
@@ -131,9 +132,9 @@ std::optional<CategorizedFile> build_categorized_entry(sqlite3_stmt* stmt) {
     std::string dir_path = file_dir_path ? file_dir_path : "";
     std::string name = file_name ? file_name : "";
     std::string type_str = file_type ? file_type : "";
-    std::string cat = category ? category : "";
-    std::string subcat = subcategory ? subcategory : "";
-    std::string suggested = suggested_name ? suggested_name : "";
+    std::string cat = Utils::sanitize_path_label(category ? category : "");
+    std::string subcat = Utils::sanitize_path_label(subcategory ? subcategory : "");
+    std::string suggested = Utils::sanitize_path_label(suggested_name ? suggested_name : "");
 
     int taxonomy_id = 0;
     if (sqlite3_column_count(stmt) > 6 && sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `501988f`
```diff
diff --git a/TESTS.md b/TESTS.md
--- a/TESTS.md
+++ b/TESTS.md
@@ -151,6 +151,13 @@ Procedure: Call `Utils::abbreviate_user_path()` on the full path.
 Expected outcome: The returned string omits the home prefix and begins with `Documents/`.
 Run: `./build-tests/ai_file_sorter_tests "abbreviate_user_path strips home prefix"`
 
+#### Test case: sanitize_path_label strips invalid UTF-8 bytes
+Purpose: Ensure path labels remain valid UTF-8 even when upstream text contains malformed byte sequences.
+Setup: Build a string containing an invalid UTF-8 byte between otherwise valid ASCII text.
+Procedure: Call `Utils::sanitize_path_label()`.
+Expected outcome: The invalid byte is removed and the returned label remains valid UTF-8 text.
+Run: `./build-tests/ai_file_sorter_tests "sanitize_path_label strips invalid UTF-8 bytes"`
+
 ### `tests/unit/test_llm_selection_dialog_visual.cpp` (non-Windows only)
 
 #### Test case: Visual LLaVA entry shows missing env var state
@@ -277,6 +284,13 @@ Procedure: Call `remove_empty_categorizations()` and then fetch categorized file
 Expected outcome: Only the truly empty entry is removed; the rename-only entry remains with empty category labels and the suggestion intact.
 Run: `./build-tests/ai_file_sorter_tests "DatabaseManager keeps rename-only entries with empty labels"`
 
+#### Test case: DatabaseManager sanitizes invalid UTF-8 in cached labels
+Purpose: Ensure malformed UTF-8 in cached category labels or suggestions does not propagate into the review dialog pipeline.
+Setup: Insert a cached entry whose category, subcategory, and suggested filename contain invalid UTF-8 bytes.
+Procedure: Fetch categorized files from the database.
+Expected outcome: The loaded category, subcategory, and suggested name are returned with invalid UTF-8 bytes removed.
+Run: `./build-tests/ai_file_sorter_tests "DatabaseManager sanitizes invalid UTF-8 in cached labels"`
+
 #### Test case: DatabaseManager normalizes subcategory stopword suffixes for taxonomy matching
 Purpose: Verify taxonomy resolution normalizes stopword suffixes like "files".
 Setup: Resolve categories with and without the "files" suffix (e.g., "Graphics" vs "Graphics files").
```

This second excerpt is included because `501988f` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
