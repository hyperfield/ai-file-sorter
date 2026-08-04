# 2025-10-19: Logging expansion, taxonomy consistency, and path-aware categorization context

## Covered commits
- `54bcaa3` `2025-10-18` `feat(logging): add more logging coverage`
- `bc76088` `2025-10-18` `feat(categorization): add taxonomy for more categorization consistency`
- `e5e294f` `2025-10-19` `feat(categorization): add paths to file names in categorization requests to LLM`
- `639d4ce` `2025-10-19` `feat(logging): improve logging coverage and methodology`
- `5d4e831` `2025-10-19` `feat(CategorizationProgressDialog): improve the readability`
- `7c8884b` `2025-10-19` `fix(cuda): issue with engaging CUDA after llama.cpp update`
- `4d4de4e` `2025-10-19` `fix(cuda-windows): improve the CUDA driver detection algorithm under Windows`
- `3f4538b` `2025-10-19` `fix(cuda): increase realibility`
- `2b94673` `2025-10-19` `chore(version): app version update`

## Motivation
The October 2025 work tightened the core categorization engine before the Qt6-era feature burst. The app needed better diagnostics for misclassifications and GPU/runtime problems, and categorization requests needed richer context so similarly named files in different folders would no longer be treated as if they were equivalent.

## What changed
The grouped commits expanded structured logging, added a taxonomy layer for more consistent categories, and started including directory-path context in categorization prompts. CUDA detection on Windows was also hardened in the same slice because those runtime failures were hard to interpret without the new logging coverage.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `54bcaa3`
```diff
diff --git a/app/lib/CategorizationSession.cpp b/app/lib/CategorizationSession.cpp
--- a/app/lib/CategorizationSession.cpp
+++ b/app/lib/CategorizationSession.cpp
@@ -1,5 +1,6 @@
 #include "CategorizationSession.hpp"
 #include "CryptoManager.hpp"
+#include "Logger.hpp"
 #include <stdexcept>
 #include <iostream>
 #include <algorithm> // For std::fill
@@ -12,11 +13,24 @@ CategorizationSession::CategorizationSession()
     const char* env_rr = std::getenv("ENV_RR");
 
     if (!env_pc || !env_rr) {
+        if (auto logger = Logger::get_logger("core_logger")) {
+            logger->error("Missing environment variables for key decryption. ENV_PC present: {}, ENV_RR present: {}",
+                          env_pc ? "yes" : "no",
+                          env_rr ? "yes" : "no");
+        }
         throw std::runtime_error("Missing environment variables for key decryption");
     }
 
+    if (auto logger = Logger::get_logger("core_logger")) {
+        logger->debug("Reconstructing API key using embedded environment variables.");
+    }
+
     CryptoManager crypto(env_pc, env_rr);
     key = crypto.reconstruct();
+
+    if (auto logger = Logger::get_logger("core_logger")) {
+        logger->info("API key reconstructed successfully from embedded environment variables.");
+    }
 }
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `bc76088`
```diff
diff --git a/app/include/DatabaseManager.hpp b/app/include/DatabaseManager.hpp
--- a/app/include/DatabaseManager.hpp
+++ b/app/include/DatabaseManager.hpp
@@ -5,6 +5,7 @@
 #include <string>
 #include <map>
 #include <vector>
+#include <unordered_map>
 #include <sqlite3.h>
 
 class DatabaseManager {
@@ -13,19 +14,53 @@ public:
     ~DatabaseManager();
 
     bool is_file_already_categorized(const std::string &file_name);
+    struct ResolvedCategory {
+        int taxonomy_id;
+        std::string category;
+        std::string subcategory;
+    };
+
+    ResolvedCategory resolve_category(const std::string& category,
+                                      const std::string& subcategory);
+
     bool insert_or_update_file_with_categorization(const std::string& file_name,
                                                    const std::string& file_type,
-                                                   const std::string& dir_path, 
-                                                   const std::string& category, 
-                                                   const std::string& subcategory);
+                                                   const std::string& dir_path,
+                                                   const ResolvedCategory& resolved);
     std::vector<std::string> get_dir_contents_from_db(const std::string &dir_path);
 
     std::vector<CategorizedFile> get_categorized_files(const std::string &directory_path);
 
     std::vector<std::string>
         get_categorization_from_db(const std::string& file_name, const FileType file_type);
+    void increment_taxonomy_frequency(int taxonomy_id);
 
 private:
+    struct TaxonomyEntry {
+        int id;
+        std::string category;
+        std::string subcategory;
+        std::string normalized_category;
+        std::string normalized_subcategory;
+        int frequency;
+    };
+
+    void initialize_schema();
+    void initialize_taxonomy_schema();
+    void load_taxonomy_cache();
+    std::string normalize_label(const std::string& input) const;
+    static double string_similarity(const std::string& a, const std::string& b);
+    static std::string make_key(const std::string& norm_category,
+                                const std::string& norm_subcategory);
+    int create_taxonomy_entry(const std::string& category,
+                              const std::string& subcategory,
+                              const std::string& norm_category,
+                              const std::string& norm_subcategory);
+    void ensure_alias_mapping(int taxonomy_id,
+                              const std::string& norm_category,
+                              const std::string& norm_subcategory);
+    const TaxonomyEntry* find_taxonomy_entry(int taxonomy_id) const;
+
     std::map<std::string, std::string> cached_results;
     std::string get_cached_category(const std::string &file_name);
     void load_cache();
```

This second excerpt is included because `bc76088` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
