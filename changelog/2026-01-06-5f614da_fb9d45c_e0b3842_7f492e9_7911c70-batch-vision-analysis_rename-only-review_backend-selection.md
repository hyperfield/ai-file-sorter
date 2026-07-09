# 2026-01-06: Visual inference integration, rename-only review flow, and backend selection plumbing

## Covered commits
- `5f614da` `2026-01-06` `feat(vision): integrate LLaVA analysis with prompt overrides and progress`
- `fb9d45c` `2026-01-06` `feat(review): support rename-only with cached suggestions`
- `e0b3842` `2026-01-06` `feat(gpu): add backend selection and shared model params`
- `7f492e9` `2026-01-06` `test: add rename-only and cache coverage`
- `7911c70` `2026-01-06` `chore(build): fix mtmd build helpers and register tests`
- `b575927` `2026-01-06` `docs(readme): document runtime environment variables`

## Motivation
After the model-download UI existed, the core image-analysis pipeline had to be threaded into the app. The product needed real LLaVA inference, a review path for rename-only image handling, and more explicit runtime backend selection so the new vision workload could be routed predictably.

## What changed
These commits integrated LLaVA analysis into the processing pipeline, added rename-only review behavior with cache coverage, introduced backend-selection/shared-model parameters, registered the new tests in the build, and documented the runtime environment controls for operators.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `5f614da`
```diff
diff --git a/app/include/CategorizationService.hpp b/app/include/CategorizationService.hpp
--- a/app/include/CategorizationService.hpp
+++ b/app/include/CategorizationService.hpp
@@ -24,6 +24,11 @@ public:
     using ProgressCallback = std::function<void(const std::string&)>;
     using QueueCallback = std::function<void(const FileEntry&)>;
     using RecategorizationCallback = std::function<void(const CategorizedFile&, const std::string&)>;
+    struct PromptOverride {
+        std::string name;
+        std::string path;
+    };
+    using PromptOverrideProvider = std::function<std::optional<PromptOverride>(const FileEntry&)>;
 
     CategorizationService(Settings& settings,
                           DatabaseManager& db_manager,
@@ -40,7 +45,8 @@ public:
         const ProgressCallback& progress_callback,
         const QueueCallback& queue_callback,
         const RecategorizationCallback& recategorization_callback,
-        std::function<std::unique_ptr<ILLMClient>()> llm_factory) const;
+        std::function<std::unique_ptr<ILLMClient>()> llm_factory,
+        const PromptOverrideProvider& prompt_override = {}) const;
 
 private:
     using CategoryPair = std::pair<std::string, std::string>;
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `fb9d45c`
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -49,12 +49,29 @@ private:
     enum class RowStatus {
         None = 0,
         Moved,
+        Renamed,
+        RenamedAndMoved,
         Skipped,
         NotSelected,
         Preview
     };
 
     static constexpr int kStatusRole = Qt::UserRole + 100;
+    static constexpr int kFilePathRole = Qt::UserRole + 1;
+    static constexpr int kUsedConsistencyRole = Qt::UserRole + 2;
+    static constexpr int kRenameOnlyRole = Qt::UserRole + 3;
+    static constexpr int kFileTypeRole = Qt::UserRole + 4;
+
+    enum Column {
+        ColumnSelect = 0,
+        ColumnFile = 1,
+        ColumnType = 2,
+        ColumnSuggestedName = 3,
+        ColumnCategory = 4,
+        ColumnSubcategory = 5,
+        ColumnStatus = 6,
+        ColumnPreview = 7
+    };
 
     struct MoveRecord {
         int row_index;
@@ -66,10 +83,12 @@ private:
     struct PreviewRecord {
         std::string source;
         std::string destination;
-        std::string file_name;
+        std::string source_file_name;
+        std::string destination_file_name;
         std::string category;
         std::string subcategory;
         bool use_subcategory{false};
+        bool rename_only{false};
     };
 
     void setup_ui();
```

This second excerpt is included because `fb9d45c` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
