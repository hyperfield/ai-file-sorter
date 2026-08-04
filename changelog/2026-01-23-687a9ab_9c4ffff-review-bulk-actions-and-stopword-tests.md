# 2026-01-23: Review-dialog bulk actions and stopword matching tests

## Covered commits
- `687a9ab` `2026-01-23` `feat(review): add bulk actions and keep buttons visible on small screens`
- `9c4ffff` `2026-01-23` `test(taxonomy): cover stopword subcategory matching`
- `6981da7` `2026-01-23` `chore(documentation): add doxygen headers`

## Motivation
As review workloads grew, editing one row at a time stopped scaling. The UI needed bulk actions and more resilient small-screen behavior, and taxonomy logic needed targeted tests for stopword handling so bulk-edited outputs still converged toward consistent labels.

## What changed
The grouped work added review-dialog bulk actions, kept critical buttons visible on smaller screens, expanded taxonomy tests around stopword matching, and followed up with more Doxygen-style documentation in adjacent headers.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `687a9ab`
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -7,6 +7,8 @@
 - Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
 - Image rename suggestions are now saved as you go, so progress isn't lost if the app closes unexpectedly.
 - Image analysis now falls back (with a user prompt) to CPU if the GPU has insufficient available memory.
+- Review dialog now lets you select highlighted rows and bulk edit their categories.
+- Review dialog is now scrollable on smaller screens so action buttons stay visible.
 - Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
 
 ## [1.5.0] - 2026-01-11
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
index cf79ac3..064ad81 100644
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -108,7 +108,15 @@ private:
                               bool renamed = false,
                               bool moved = false);
     void on_select_all_toggled(bool checked);
+    /**
+     * @brief Selects all highlighted rows for processing.
+     */
+    void on_select_highlighted_clicked();
     void apply_select_all(bool checked);
+    /**
+     * @brief Applies a check state to the given rows in the Process column.
+     */
+    void apply_check_state_to_rows(const std::vector<int>& rows, Qt::CheckState state);
     void on_item_changed(QStandardItem* item);
     void update_select_all_state();
     void update_type_icon(QStandardItem* item);
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `9c4ffff`
```diff
diff --git a/tests/unit/test_database_manager_rename_only.cpp b/tests/unit/test_database_manager_rename_only.cpp
--- a/tests/unit/test_database_manager_rename_only.cpp
+++ b/tests/unit/test_database_manager_rename_only.cpp
@@ -30,3 +30,20 @@ TEST_CASE("DatabaseManager keeps rename-only entries with empty labels") {
     CHECK(entries.front().category.empty());
     CHECK(entries.front().subcategory.empty());
 }
+
+TEST_CASE("DatabaseManager normalizes subcategory stopword suffixes for taxonomy matching") {
+    TempDir base_dir;
+    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
+    DatabaseManager db(base_dir.path().string());
+
+    auto base = db.resolve_category("Images", "Graphics");
+    auto with_suffix = db.resolve_category("Images", "Graphics files");
+
+    REQUIRE(base.taxonomy_id > 0);
+    CHECK(with_suffix.taxonomy_id == base.taxonomy_id);
+    CHECK(with_suffix.category == base.category);
+    CHECK(with_suffix.subcategory == base.subcategory);
+
+    auto photos = db.resolve_category("Images", "Photos");
+    CHECK(photos.subcategory == "Photos");
+}
```

This second excerpt is included because `9c4ffff` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
