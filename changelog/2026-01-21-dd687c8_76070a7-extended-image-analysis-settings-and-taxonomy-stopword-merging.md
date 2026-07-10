# 2026-01-21: Expanded image-analysis settings and taxonomy-driven subcategory merging

## Covered commits
- `dd687c8` `2026-01-21` `feat(app): extend LLM settings and image analysis flow`
- `76070a7` `2026-01-21` `feat(taxonomy): merge subcategory labels with generic suffixes`
- `4d303e6` `2026-01-21` `chore(submodules): ignore dirty llama.cpp and update pointer`

## Motivation
With image analysis and rename-only behavior established, the app still needed a broader settings surface for that workflow and a way to collapse near-duplicate subcategory labels. Otherwise users could get technically different but practically identical folders from the same run.

## What changed
These commits extended LLM/image-analysis settings and introduced taxonomy logic that merges subcategory labels differing only by generic suffixes, while updating the llama.cpp submodule pointer to support the new flow.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `dd687c8`
```diff
diff --git a/app/include/CategorizationService.hpp b/app/include/CategorizationService.hpp
--- a/app/include/CategorizationService.hpp
+++ b/app/include/CategorizationService.hpp
@@ -29,6 +29,8 @@ public:
         std::string path;
     };
     using PromptOverrideProvider = std::function<std::optional<PromptOverride>(const FileEntry&)>;
+    /** Supplies an optional suggested rename for an entry during categorization. */
+    using SuggestedNameProvider = std::function<std::string(const FileEntry&)>;
 
     CategorizationService(Settings& settings,
                           DatabaseManager& db_manager,
@@ -46,7 +48,8 @@ public:
         const QueueCallback& queue_callback,
         const RecategorizationCallback& recategorization_callback,
         std::function<std::unique_ptr<ILLMClient>()> llm_factory,
-        const PromptOverrideProvider& prompt_override = {}) const;
+        const PromptOverrideProvider& prompt_override = {},
+        const SuggestedNameProvider& suggested_name_provider = {}) const;
 
 private:
     using CategoryPair = std::pair<std::string, std::string>;
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `76070a7`
```diff
diff --git a/app/lib/DatabaseManager.cpp b/app/lib/DatabaseManager.cpp
--- a/app/lib/DatabaseManager.cpp
+++ b/app/lib/DatabaseManager.cpp
@@ -10,6 +10,7 @@
 #include <memory>
 #include <optional>
 #include <sstream>
+#include <unordered_set>
 #include <utility>
 #include <vector>
 
@@ -375,6 +376,44 @@ std::string DatabaseManager::normalize_label(const std::string &input) const {
     return result;
 }
 
+static std::string strip_trailing_stopwords(const std::string& normalized) {
+    if (normalized.empty()) {
+        return normalized;
+    }
+    static const std::unordered_set<std::string> kStopwords = {
+        "file", "files",
+        "doc", "docs", "document", "documents",
+        "image", "images",
+        "photo", "photos",
+        "pic", "pics"
+    };
+
+    std::istringstream iss(normalized);
+    std::vector<std::string> tokens;
+    std::string token;
+    while (iss >> token) {
+        tokens.push_back(token);
+    }
+    if (tokens.size() <= 1) {
+        return normalized;
+    }
+    while (tokens.size() > 1 && kStopwords.contains(tokens.back())) {
+        tokens.pop_back();
+    }
+    if (tokens.empty()) {
+        return normalized;
+    }
+
+    std::string joined;
+    for (size_t index = 0; index < tokens.size(); ++index) {
+        if (index > 0) {
+            joined.push_back(' ');
+        }
+        joined += tokens[index];
+    }
+    return joined;
+}
+
 double DatabaseManager::string_similarity(const std::string &a, const std::string &b) {
     if (a == b) {
         return 1.0;
```

This second excerpt is included because `76070a7` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
