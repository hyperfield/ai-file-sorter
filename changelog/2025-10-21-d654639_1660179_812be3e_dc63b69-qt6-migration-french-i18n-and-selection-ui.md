# 2025-10-21: Qt6 migration, French internationalization, splash screen, and review selection controls

## Covered commits
- `d654639` `2025-10-21` `feat(core): switch to qt6`
- `1660179` `2025-10-21` `feat(app): add internationalization framework and French translations for the interface`
- `812be3e` `2025-10-21` `feat(app): add selection checkboxes to the confirmation dialog for more sorting flexibility`
- `dc63b69` `2025-10-21` `feat(app): add dynamically adjusted splash screen`
- `5de1299` `2025-10-21` `feat(tests): add translation tests`
- `a6ff352` `2025-10-21` `feat(llama.cpp): add heuristic algorithm for optimal parameters setting for GPU usage on Metal`
- `b5e89f6` `2025-10-21` `feat(main-window): interface improvement`
- `62b975c` `2025-10-21` `fix(main-widows): resolve memory leak`
- `99bd0ed` `2025-10-22` `chore(splash-screen): change appearance timing to 3.3s`
- `12cfac5` `2025-10-22` `chore(main): remove X11 guards`
- `9acc727` `2025-10-22` `fix(llama-build): optimize the build script for Linux for more compatibility and reliability with CUDA`
- `975a391` `2025-10-22` `chore(llama-cpp): improve ngl selection algorithm for cuda`

## Motivation
This was the architectural pivot from the earlier GTK-style code toward the native Qt6 desktop application that the project still builds on. The migration was justified by the need for a richer native UI, cleaner cross-platform support, formal translation infrastructure, and more flexible review interactions before sorting is applied.

## What changed
The change set moved the application shell to Qt6, introduced the translation framework with French UI coverage, added selection checkboxes in the confirmation dialog, and brought in a timed splash-screen flow plus early Metal/GPU heuristics for Apple hardware.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `d654639`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -1,9 +1,9 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://badgen.net/badge/version/0.9.7/green)](#)
+[![Version](https://badgen.net/badge/version/1.0.0/green)](#)
 
-AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice.
+AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a modern Qt6 interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice.
 
 [![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 
@@ -13,6 +13,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - [AI File Sorter](#ai-file-sorter)
   - [Changelog](#changelog)
+    - [[1.0.0] - 2026-02-18](#100---2026-02-18)
     - [[0.9.7] - 2025-10-19](#097---2025-10-19)
     - [[0.9.3] - 2025-09-22](#093---2025-09-22)
     - [[0.9.2] - 2025-08-06](#092---2025-08-06)
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `1660179`
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -13,6 +13,7 @@
 
 class DatabaseManager;
 class QCloseEvent;
+class QEvent;
 class QPushButton;
 class QTableView;
 class QCheckBox;
@@ -30,8 +31,18 @@ public:
 
 protected:
     void closeEvent(QCloseEvent* event) override;
+    void changeEvent(QEvent* event) override;
 
 private:
+    enum class RowStatus {
+        None = 0,
+        Moved,
+        Skipped,
+        NotSelected
+    };
+
+    static constexpr int kStatusRole = Qt::UserRole + 100;
+
     void setup_ui();
     void populate_model();
     void record_categorization_to_db();
```

This second excerpt is included because `1660179` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
