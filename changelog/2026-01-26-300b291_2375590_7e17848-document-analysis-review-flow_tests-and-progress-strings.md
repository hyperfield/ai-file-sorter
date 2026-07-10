# 2026-01-26: Document-analysis review flow, test/build wiring, and cached-suggestion progress strings

## Covered commits
- `300b291` `2026-01-26` `feat(docs): add document analysis + rename-only/cache reuse in review flow`
- `2375590` `2026-01-26` `test(app): wire document analysis into tests/build`
- `7e17848` `2026-01-26` `i18n(app): add cached-suggestion progress strings`
- `157e7db` `2026-01-26` `chore(i18n): update qm binaries`

## Motivation
Once document-analysis controls existed, the review flow and tests still needed to understand document rename-only behavior as a first-class mode. Progress output also had to acknowledge cached suggestions so long runs communicated whether work was being recomputed or reused.

## What changed
This grouped slice wired document analysis through the tests and build, updated the review/documentation flow for rename-only and cache reuse, added progress strings for cached suggestions, and refreshed compiled translation artifacts afterward.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `300b291`
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -160,9 +160,21 @@ private:
     void apply_category_visibility();
     void apply_rename_only_row_visibility();
     void update_rename_only_checkbox_state();
+    /**
+     * @brief Enables/disables the subcategory checkbox based on rename-only mode.
+     */
+    void update_subcategory_checkbox_state();
     void on_rename_images_only_toggled(bool checked);
+    /**
+     * @brief Marks document rows as rename-only when toggled.
+     */
+    void on_rename_documents_only_toggled(bool checked);
     bool row_is_already_renamed_with_category(int row) const;
     bool row_is_supported_image(int row) const;
+    /**
+     * @brief Returns true if the row points to a supported document file.
+     */
+    bool row_is_supported_document(int row) const;
     /**
      * @brief Returns unique row indices that are highlighted in the table view.
      */
@@ -192,6 +204,7 @@ private:
     QCheckBox* show_subcategories_checkbox{nullptr};
     QCheckBox* dry_run_checkbox{nullptr};
     QCheckBox* rename_images_only_checkbox{nullptr};
+    QCheckBox* rename_documents_only_checkbox{nullptr};
     QPushButton* undo_button{nullptr};
 
     std::vector<MoveRecord> move_history_;
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `2375590`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -45,7 +45,7 @@ find_package(Intl REQUIRED) # libintl/gettext
 
 # Sources
 set(APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
-file(GLOB APP_LIB_SOURCES
+file(GLOB APP_LIB_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
 )
 set(APP_SOURCES ${APP_MAIN_SOURCE} ${APP_LIB_SOURCES})
diff --git a/app/include/MainAppTestAccess.hpp b/app/include/MainAppTestAccess.hpp
index e574f4a..dc5bb34 100644
--- a/app/include/MainAppTestAccess.hpp
+++ b/app/include/MainAppTestAccess.hpp
@@ -24,10 +24,14 @@ public:
     static QCheckBox* rename_images_only_checkbox(MainApp& app);
     static void split_entries_for_analysis(const std::vector<FileEntry>& files,
                                            bool analyze_images,
+                                           bool analyze_documents,
                                            bool process_images_only,
+                                           bool process_documents_only,
                                            bool rename_images_only,
+                                           bool rename_documents_only,
                                            const std::unordered_set<std::string>& renamed_files,
                                            std::vector<FileEntry>& image_entries,
+                                           std::vector<FileEntry>& document_entries,
                                            std::vector<FileEntry>& other_entries);
     static void set_visual_llm_available_probe(MainApp& app, std::function<bool()> probe);
     static void set_llm_selection_runner(MainApp& app, std::function<void()> runner);
```

This second excerpt is included because `2375590` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
