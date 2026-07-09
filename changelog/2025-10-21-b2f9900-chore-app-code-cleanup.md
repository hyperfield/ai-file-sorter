# 2025-10-21: chore(app): code cleanup

## Covered commits
- `b2f9900` `2025-10-21` `chore(app): code cleanup`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/DatabaseManager.hpp`
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/DatabaseManager.hpp`, `app/lib/DatabaseManager.cpp`, `app/lib/Utils.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `b2f9900`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/DatabaseManager.hpp b/app/include/DatabaseManager.hpp
--- a/app/include/DatabaseManager.hpp
+++ b/app/include/DatabaseManager.hpp
@@ -52,6 +52,16 @@ private:
     static double string_similarity(const std::string& a, const std::string& b);
     static std::string make_key(const std::string& norm_category,
                                 const std::string& norm_subcategory);
+    std::pair<int, double> find_fuzzy_match(const std::string& norm_category,
+                                            const std::string& norm_subcategory) const;
+    int resolve_existing_taxonomy(const std::string& key,
+                                   const std::string& norm_category,
+                                   const std::string& norm_subcategory) const;
+    ResolvedCategory build_resolved_category(int taxonomy_id,
+                                             const std::string& fallback_category,
+                                             const std::string& fallback_subcategory,
+                                             const std::string& norm_category,
+                                             const std::string& norm_subcategory);
     int create_taxonomy_entry(const std::string& category,
                               const std::string& subcategory,
                               const std::string& norm_category,
diff --git a/app/lib/DatabaseManager.cpp b/app/lib/DatabaseManager.cpp
index 8f1d231..116d561 100644
--- a/app/lib/DatabaseManager.cpp
+++ b/app/lib/DatabaseManager.cpp
@@ -385,6 +385,80 @@ const DatabaseManager::TaxonomyEntry *DatabaseManager::find_taxonomy_entry(int t
     return &taxonomy_entries[idx];
 }
 
+std::pair<int, double> DatabaseManager::find_fuzzy_match(
+    const std::string& norm_category,
+    const std::string& norm_subcategory) const {
+    if (taxonomy_entries.empty()) {
+        return {-1, 0.0};
+    }
+
+    double best_score = 0.0;
+    int best_id = -1;
+    for (const auto &entry : taxonomy_entries) {
+        double category_score = string_similarity(norm_category, entry.normalized_category);
+        double subcategory_score =
+            string_similarity(norm_subcategory, entry.normalized_subcategory);
+        double combined = (category_score + subcategory_score) / 2.0;
+        if (combined > best_score) {
+            best_score = combined;
+            best_id = entry.id;
+        }
+    }
+
+    if (best_id != -1 && best_score >= kSimilarityThreshold) {
+        return {best_id, best_score};
+    }
+    return {-1, best_score};
+}
+
+int DatabaseManager::resolve_existing_taxonomy(const std::string& key,
+                                               const std::string& norm_category,
+                                               const std::string& norm_subcategory) const {
+    auto alias_it = alias_lookup.find(key);
+    if (alias_it != alias_lookup.end()) {
+        return alias_it->second;
+    }
+
+    auto canonical_it = canonical_lookup.find(key);
+    if (canonical_it != canonical_lookup.end()) {
+        return canonical_it->second;
+    }
+
+    auto [best_id, score] = find_fuzzy_match(norm_category, norm_subcategory);
+    return best_id;
+}
+
+DatabaseManager::ResolvedCategory DatabaseManager::build_resolved_category(
+    int taxonomy_id,
+    const std::string& fallback_category,
+    const std::string& fallback_subcategory,
+    const std::string& norm_category,
+    const std::string& norm_subcategory) {
+
+    ResolvedCategory result{-1, fallback_category, fallback_subcategory};
+
+    if (taxonomy_id == -1) {
+        taxonomy_id = create_taxonomy_entry(fallback_category, fallback_subcategory,
+                                            norm_category, norm_subcategory);
+    }
+
+    if (taxonomy_id != -1) {
+        ensure_alias_mapping(taxonomy_id, norm_category, norm_subcategory);
+        if (const auto *entry = find_taxonomy_entry(taxonomy_id)) {
+            result.taxonomy_id = entry->id;
+            result.category = entry->category;
+            result.subcategory = entry->subcategory;
+        } else {
+            result.taxonomy_id = taxonomy_id;
+        }
+    } else {
+        result.category = fallback_category;
+        result.subcategory = fallback_subcategory;
+    }
+
+    return result;
```

The excerpt is taken from the commit diff for `chore(app): code cleanup`. The most relevant surfaces are `app/include/DatabaseManager.hpp`, `app/lib/DatabaseManager.cpp`, `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
