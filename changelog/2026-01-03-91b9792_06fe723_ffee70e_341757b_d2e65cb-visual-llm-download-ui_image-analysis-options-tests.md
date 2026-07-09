# 2026-01-03: LLaVA download UI, image-analysis settings, and early test coverage

## Covered commits
- `91b9792` `2026-01-03` `feat(visual-llm): add LLaVA download UI and resources`
- `06fe723` `2026-01-03` `feat(image-analysis): add settings and UI toggles`
- `ffee70e` `2026-01-03` `test(image-analysis): cover image options and labels`
- `341757b` `2026-01-03` `test(visual-llm): cover LLaVA download UI states`
- `d2e65cb` `2026-01-03` `chore(ui): add translations for new labels`

## Motivation
This was the first dedicated visual-LLM milestone. The app needed a concrete way to download and configure the model files required for image analysis, and the UI needed explicit toggles for picture-specific processing before the actual visual inference path could be integrated safely.

## What changed
The grouped commits added the LLaVA download controls and assets, exposed image-analysis settings in the main UI, added tests for the new image options and download states, and rounded out the translation keys required for the new controls.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `91b9792`
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -124,6 +124,9 @@ endif
 QRC_FILE := resources/app.qrc
 QRC_CPP := $(OBJ_DIR)/qrc_app.cpp
 QRC_OBJ := $(OBJ_DIR)/qrc_app.o
+QRC_DIR := $(dir $(QRC_FILE))
+QRC_RESOURCES := $(shell sed -n 's/.*<file>\\(.*\\)<\\/file>.*/\\1/p' $(QRC_FILE))
+QRC_RESOURCES := $(addprefix $(QRC_DIR),$(QRC_RESOURCES))
 
 ifndef RCC
 RCC := $(shell command -v qt6-rcc 2>/dev/null)
@@ -175,7 +178,7 @@ $(OBJ_DIR)/main.o: main.cpp
 	mkdir -p $(OBJ_DIR)
 	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@
 
-$(QRC_CPP): $(QRC_FILE)
+$(QRC_CPP): $(QRC_FILE) $(QRC_RESOURCES)
 	mkdir -p $(OBJ_DIR)
 	$(RCC) -o $@ $<
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `06fe723`
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -108,9 +108,14 @@ private:
     void initialize_whitelists();
 
     void on_analyze_clicked();
-    void on_directory_selected(const QString& path, bool user_initiated = false);
+    void on_directory_selected(const QString& path,
+        bool user_initiated = false);
     void ensure_one_checkbox_active(QCheckBox* changed_checkbox);
     void update_file_scan_option(FileScanOptions option, bool enabled);
+    bool visual_llm_files_available() const;
+    void update_image_analysis_controls();
+    void handle_image_analysis_toggle(bool checked);
+    void run_llm_selection_dialog_for_visual();
     void update_analyze_button_state(bool analyzing);
     void update_results_view_mode();
     void update_folder_contents(const QString& directory);
@@ -177,6 +182,9 @@ private:
     QPointer<QComboBox> whitelist_selector;
     QPointer<QCheckBox> categorize_files_checkbox;
     QPointer<QCheckBox> categorize_directories_checkbox;
+    QPointer<QCheckBox> analyze_images_checkbox;
+    QPointer<QCheckBox> offer_rename_images_checkbox;
+    QPointer<QCheckBox> rename_images_only_checkbox;
     QPointer<QTreeView> tree_view;
     QPointer<QStandardItemModel> tree_model;
     QPointer<QStackedWidget> results_stack;
```

This second excerpt is included because `06fe723` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
