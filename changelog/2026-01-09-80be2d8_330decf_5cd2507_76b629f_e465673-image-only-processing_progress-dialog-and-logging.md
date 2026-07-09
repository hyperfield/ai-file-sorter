# 2026-01-09: Image-only processing mode, progress-dialog polish, and LLaVA logging follow-up

## Covered commits
- `80be2d8` `2026-01-09` `feat(ui): add image-only processing option`
- `330decf` `2026-01-09` `chore(ui): update categorization progress dialog`
- `5cd2507` `2026-01-09` `chore(logs): log LLaVA filename suggestions`
- `76b629f` `2026-01-09` `chore(ui): update confirm button label`
- `e465673` `2026-01-09` `docs: document image-only processing option`
- `a0400d9` `2026-01-09` `docs: document image-only processing option`

## Motivation
Once image analysis was live, users needed a way to focus a run exclusively on picture files instead of mixing them into the generic categorization path. The progress dialog and review copy also needed to reflect the new mode clearly, and developers needed better logging for filename-suggestion troubleshooting.

## What changed
The grouped work added the image-only processing option, refreshed categorization-progress dialog output, logged LLaVA filename suggestions, adjusted confirm-button labeling, and documented the new mode in the README.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `80be2d8`
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -114,6 +114,7 @@ private:
     void update_file_scan_option(FileScanOptions option, bool enabled);
     bool visual_llm_files_available() const;
     void update_image_analysis_controls();
+    void update_image_only_controls();
     void handle_image_analysis_toggle(bool checked);
     void run_llm_selection_dialog_for_visual();
     void update_analyze_button_state(bool analyzing);
@@ -155,6 +156,7 @@ private:
 
     void run_on_ui(std::function<void()> func);
     void changeEvent(QEvent* event) override;
+    FileScanOptions effective_scan_options() const;
 
     friend class MainAppUiBuilder;
 #ifdef AI_FILE_SORTER_TEST_BUILD
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `330decf`
```diff
diff --git a/app/lib/CategorizationProgressDialog.cpp b/app/lib/CategorizationProgressDialog.cpp
--- a/app/lib/CategorizationProgressDialog.cpp
+++ b/app/lib/CategorizationProgressDialog.cpp
@@ -92,7 +92,7 @@ void CategorizationProgressDialog::request_stop()
     if (!main_app) {
         return;
     }
-    main_app->report_progress("[STOP] Cancelling analysis...");
+    main_app->report_progress("[STOP] Analysis will stop after the current item is processed.");
     main_app->request_stop_analysis();
 }
```

This second excerpt is included because `330decf` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
