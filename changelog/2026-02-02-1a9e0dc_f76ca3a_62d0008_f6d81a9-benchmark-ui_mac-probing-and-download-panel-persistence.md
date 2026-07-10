# 2026-02-02: Benchmarking UI expansion, macOS backend probing, and LLM-download panel persistence

## Covered commits
- `1a9e0dc` `2026-02-02` `feat(ui): improve LLM selection dialog and downloads controls`
- `f76ca3a` `2026-02-02` `feat(benchmark): add stop control, styling, and configurable thresholds`
- `62d0008` `2026-02-02` `feat(benchmark): localize output and refine macOS backend probing`
- `f6d81a9` `2026-02-02` `feat(settings): persist LLM downloads panel state`
- `4dc9d56` `2026-02-02` `fix(ui): tighten LLM selection controls on macOS`
- `a16c751` `2026-02-02` `fix(ui): use macOS-friendly disclosure icons`
- `f3f1826` `2026-02-02` `fix(ui): refine benchmark recommendations copy`
- `4627027` `2026-02-02` `fix(windows): define icons_match helper`
- `5061ffa` `2026-02-02` `fix(build): stabilize libzip cmake link flags on macOS`
- `2696d72` `2026-02-02` `docs(readme): document system compatibility check`
- `b871001` `2026-02-02` `docs(build): disable libzip docs and ignore build dir`

## Motivation
The system-compatibility benchmark was becoming a user-facing guide rather than a developer toy, so it needed better controls, localized/macOS-aware probing, and tighter UI behavior in the selection dialog. State persistence for the downloads panel was also justified because the selection dialog had become a central workflow hub.

## What changed
This grouped work improved the LLM selection dialog, added benchmark stop/styling/threshold controls, localized benchmark output and macOS backend probing, persisted the downloads panel state, tightened macOS-friendly disclosure controls, refined recommendation copy, stabilized libzip link flags, and documented the benchmark flow in the README.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `1a9e0dc`
```diff
diff --git a/app/include/LLMSelectionDialog.hpp b/app/include/LLMSelectionDialog.hpp
--- a/app/include/LLMSelectionDialog.hpp
+++ b/app/include/LLMSelectionDialog.hpp
@@ -22,6 +22,9 @@ class QComboBox;
 class QListWidget;
 class QLineEdit;
 class QCheckBox;
+class QToolButton;
+class QScrollArea;
+class QShowEvent;
 
 class Settings;
 
@@ -53,6 +56,7 @@ private:
 
     struct VisualLlmDownloadEntry {
         std::string env_var;
+        std::string display_name;
         QWidget* container{nullptr};
         QLabel* title_label{nullptr};
         QLabel* remote_url_label{nullptr};
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `f76ca3a`
```diff
diff --git a/app/include/SuitabilityBenchmarkDialog.hpp b/app/include/SuitabilityBenchmarkDialog.hpp
--- /dev/null
+++ b/app/include/SuitabilityBenchmarkDialog.hpp
@@ -0,0 +1,111 @@
+#ifndef SUITABILITY_BENCHMARK_DIALOG_HPP
+#define SUITABILITY_BENCHMARK_DIALOG_HPP
+
+#include <QDialog>
+#include <QStringList>
+
+#include <atomic>
+#include <memory>
+#include <thread>
+
+class QLabel;
+class QTextEdit;
+class QProgressBar;
+class QPushButton;
+class Settings;
+class QCloseEvent;
+class QEvent;
+
+/**
+ * @brief Dialog that runs a suitability benchmark for categorization and analysis features.
+ */
+class SuitabilityBenchmarkDialog : public QDialog
+{
+public:
+    /**
+     * @brief Create a suitability benchmark dialog.
+     * @param settings Settings store used for persistence.
+     * @param parent Parent widget.
+     */
+    SuitabilityBenchmarkDialog(Settings& settings,
+                               QWidget* parent = nullptr);
+    /**
+     * @brief Destructor that joins any running benchmark thread.
+     */
+    ~SuitabilityBenchmarkDialog() override;
+
+protected:
+    /**
+     * @brief Handle language changes for translated UI strings.
+     * @param event Qt event payload.
+     */
+    void changeEvent(QEvent* event) override;
+    /**
+     * @brief Prevent closing while the benchmark is running.
+     * @param event Qt close event.
+     */
+    void closeEvent(QCloseEvent* event) override;
+
+private:
+    /**
+     * @brief Build and connect the dialog UI elements.
+     */
+    void setup_ui();
+    /**
+     * @brief Update translated strings for the dialog.
+     */
+    void retranslate_ui();
+    /**
+     * @brief Start the benchmark worker thread and clear prior output.
+     */
+    void start_benchmark();
+    /**
+     * @brief Request the benchmark to stop after the current step finishes.
+     */
+    void request_stop();
+    /**
+     * @brief Run benchmark steps off the UI thread.
+     */
+    void run_benchmark_worker();
+    /**
+     * @brief Append a line to the output view.
+     * @param text Line to display.
+     * @param is_html True when the text already contains HTML markup.
+     */
+    void append_line(const QString& text, bool is_html);
+    /**
```

This second excerpt is included because `f76ca3a` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
