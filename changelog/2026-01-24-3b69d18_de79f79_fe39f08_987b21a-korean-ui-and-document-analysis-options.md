# 2026-01-24: Korean interface language, document-analysis options, and translation test expansion

## Covered commits
- `3b69d18` `2026-01-24` `feat(ui): add Korean interface language`
- `de79f79` `2026-01-24` `feat(ui): add document analysis options and collapsible sections`
- `fe39f08` `2026-01-24` `test(i18n): cover all interface languages`
- `987b21a` `2026-01-24` `chore(build): propagate llama version to mtmd`
- `3280195` `2026-01-24` `chore(i18n): update qt ts translations for new UI strings`
- `8ef8bd3` `2026-01-24` `chore(cleanup): remove unused helpers and attributes`

## Motivation
The next wave of functionality combined product-surface growth with localization coverage. The app needed Korean UI support and a richer set of document-analysis controls, while the translation test suite and build plumbing had to grow with the new strings and mtmd integration points.

## What changed
These commits added Korean as an interface language, introduced document-analysis options and collapsible UI sections, updated Qt translation catalogs, expanded i18n test coverage, propagated the llama version into mtmd build logic, and removed stale helper code in the same cleanup pass.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `3b69d18`
```diff
diff --git a/app/include/Language.hpp b/app/include/Language.hpp
--- a/app/include/Language.hpp
+++ b/app/include/Language.hpp
@@ -10,6 +10,7 @@ enum class Language {
     Italian,
     Spanish,
     Turkish,
+    Korean,
     Dutch
 };
 
@@ -24,6 +25,8 @@ inline QString languageToString(Language language)
         return QStringLiteral("Spanish");
     case Language::Turkish:
         return QStringLiteral("Turkish");
+    case Language::Korean:
+        return QStringLiteral("Korean");
     case Language::Dutch:
         return QStringLiteral("Dutch");
     case Language::French:
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `de79f79`
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -42,6 +42,7 @@ class QFileSystemModel;
 class QLineEdit;
 class QString;
 class QPushButton;
+class QToolButton;
 class QTreeView;
 class QStackedWidget;
 class QWidget;
@@ -115,6 +116,7 @@ private:
     bool visual_llm_files_available() const;
     void update_image_analysis_controls();
     void update_image_only_controls();
+    void update_document_analysis_controls();
     void handle_image_analysis_toggle(bool checked);
     void run_llm_selection_dialog_for_visual();
     void update_analyze_button_state(bool analyzing);
```

This second excerpt is included because `de79f79` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
