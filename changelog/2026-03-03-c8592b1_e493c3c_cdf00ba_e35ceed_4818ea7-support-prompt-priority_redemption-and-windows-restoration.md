# 2026-03-03: Support-prompt priority, offline redemption, and Windows restoration of support dialogs

## Covered commits
- `c8592b1` `2026-03-01` `fix(windows): remove support dialog prompts on windows`
- `e493c3c` `2026-03-03` `fix(windows): restore support dialog prompts on windows`
- `cdf00ba` `2026-03-04` `feat(support-prompt): prioritize and emphasize support action`
- `e35ceed` `2026-03-04` `feat(support-prompt): add offline donation code redemption`
- `4818ea7` `2026-03-07` `feat(support): add already-donated path and localize support dialog text`

## Motivation
The support/donation flow needed to become more intentional rather than incidental. The project wanted a stronger support prompt, an already-donated path, and offline redemption support, but the Windows-specific UX around those prompts briefly regressed and had to be restored in the same sequence.

## What changed
This grouped chapter covers the temporary removal and restoration of support prompts on Windows, the prioritization/emphasis of the support action, offline donation-code redemption, and the localized already-donated path added around the same feature arc.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `c8592b1`
```diff
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -1923,6 +1923,10 @@ bool MainApp::perform_undo_from_plan(const QString& plan_path)
 
 MainApp::SupportPromptResult MainApp::show_support_prompt_dialog(int total_files)
 {
+#ifdef _WIN32
+    (void)total_files;
+    return SupportPromptResult::NotSure;
+#else
     QMessageBox box(this);
     box.setIcon(QMessageBox::Information);
     box.setWindowTitle(QObject::tr("Support %1").arg(app_display_name()));
@@ -1988,6 +1992,7 @@ MainApp::SupportPromptResult MainApp::show_support_prompt_dialog(int total_files
         return SupportPromptResult::CannotDonate;
     }
     return SupportPromptResult::NotSure;
+#endif
 }
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `e493c3c`
```diff
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -1923,10 +1923,6 @@ bool MainApp::perform_undo_from_plan(const QString& plan_path)
 
 MainApp::SupportPromptResult MainApp::show_support_prompt_dialog(int total_files)
 {
-#ifdef _WIN32
-    (void)total_files;
-    return SupportPromptResult::NotSure;
-#else
     QMessageBox box(this);
     box.setIcon(QMessageBox::Information);
     box.setWindowTitle(QObject::tr("Support %1").arg(app_display_name()));
@@ -1992,7 +1988,6 @@ MainApp::SupportPromptResult MainApp::show_support_prompt_dialog(int total_files
         return SupportPromptResult::CannotDonate;
     }
     return SupportPromptResult::NotSure;
-#endif
 }
```

This second excerpt is included because `e493c3c` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
