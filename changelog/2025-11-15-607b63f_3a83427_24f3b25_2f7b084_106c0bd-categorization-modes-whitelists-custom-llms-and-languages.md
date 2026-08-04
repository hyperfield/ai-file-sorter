# 2025-11-15: Categorization modes, whitelists, custom LLMs, sorting, and multilingual labels

## Covered commits
- `607b63f` `2025-11-15` `feat(categorization-ui): add categorization type switch`
- `83f29d6` `2025-11-15` `feat(cateogirzation): offer to re-categorize if previous categorization was in different categorization mode`
- `75acfc4` `2025-11-15` `feat(categorization-review-dialog): add row sorting`
- `3a83427` `2025-11-16` `feat(categorization): add category whitelists`
- `24f3b25` `2025-11-16` `feat(local-llmclient): add custom llm adition ability`
- `2f7b084` `2025-11-16` `feat(categorization): add category languages`
- `106c0bd` `2025-11-16` `feat(ui): add new interface languages`
- `71c85be` `2025-11-16` `feat(tests): add for whitelists`
- `871d3c3` `2025-11-16` `chore(translation-manager): refactor`

## Motivation
By mid-November the main gap was control: users needed ways to bias the categorizer toward either detail or consistency, constrain labels to allowed vocabularies, plug in custom local models, and render category labels in more languages. The review dialog also needed better sorting support to make larger batches manageable.

## What changed
This change set introduced the categorization-type switch, whitelist management, custom local LLM registration, multilingual category language support, additional interface languages, row sorting in the review dialog, and tests around the new whitelist behavior.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `607b63f`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -10,6 +10,7 @@
 <p align="center">
   <img src="images/platform-logos/logo-vulkan.png" alt="Vulkan" width="160">
   <img src="images/platform-logos/logo-cuda.png" alt="CUDA" width="160">
+  <img src="images/platform-logos/logo-metal.png" alt="Apple Metal" width="160">
   <img src="images/platform-logos/logo-windows.png" alt="Windows" width="160">
   <img src="images/platform-logos/logo-macos.png" alt="macOS" width="160">
   <img src="images/platform-logos/logo-linux.png" alt="Linux" width="160">
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
index cc0232a..55619fb 100644
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -31,6 +31,7 @@
 
 class QAction;
 class QCheckBox;
+class QRadioButton;
 class QDockWidget;
 class QFileSystemModel;
 class QLineEdit;
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `83f29d6`
```diff
diff --git a/app/include/DatabaseManager.hpp b/app/include/DatabaseManager.hpp
--- a/app/include/DatabaseManager.hpp
+++ b/app/include/DatabaseManager.hpp
@@ -27,7 +27,8 @@ public:
     bool insert_or_update_file_with_categorization(const std::string& file_name,
                                                    const std::string& file_type,
                                                    const std::string& dir_path,
-                                                   const ResolvedCategory& resolved);
+                                                   const ResolvedCategory& resolved,
+                                                   bool used_consistency_hints);
     std::vector<std::string> get_dir_contents_from_db(const std::string &dir_path);
     bool remove_file_categorization(const std::string& dir_path,
                                     const std::string& file_name,
@@ -45,6 +46,8 @@ public:
         get_recent_categories_for_extension(const std::string& extension,
                                             FileType file_type,
                                             std::size_t limit) const;
+    bool clear_directory_categorizations(const std::string& dir_path);
+    std::optional<bool> get_directory_categorization_style(const std::string& dir_path) const;
 
 private:
     struct TaxonomyEntry {
```

This second excerpt is included because `83f29d6` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
