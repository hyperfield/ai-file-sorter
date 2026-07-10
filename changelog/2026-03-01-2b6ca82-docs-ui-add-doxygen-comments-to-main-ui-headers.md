# 2026-03-01: docs(ui): add Doxygen comments to main UI headers

## Covered commits
- `2b6ca82` `2026-03-01` `docs(ui): add Doxygen comments to main UI headers`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/include/MainAppUiBuilder.hpp`
- `M` `app/include/UiTranslator.hpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/include/MainApp.hpp`, `app/include/MainAppUiBuilder.hpp`, `app/include/UiTranslator.hpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `2b6ca82`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -59,25 +59,73 @@ struct FileEntry;
 class MainAppTestAccess;
 #endif
 
+/**
+ * @brief Main Qt window coordinating scanning, categorization, and review flows.
+ */
 class MainApp : public QMainWindow
 {
 public:
+    /**
+     * @brief Outcome returned from the optional support prompt flow.
+     */
     enum class SupportPromptResult { Support, NotSure, CannotDonate };
+    /**
+     * @brief Constructs the main application window.
+     * @param settings Persistent settings store used by the window.
+     * @param development_mode True to enable development-only UI features.
+     * @param parent Optional parent widget.
+     */
     explicit MainApp(Settings& settings, bool development_mode, QWidget* parent = nullptr);
+    /**
+     * @brief Destroys the main window and releases owned resources.
+     */
     ~MainApp() override;
 
+    /**
+     * @brief Shows the main window and starts normal interactive use.
+     */
     void run();
+    /**
+     * @brief Requests application shutdown and stops any active analysis work.
+     */
     void shutdown();
 
+    /**
+     * @brief Opens the results review dialog for a completed categorization batch.
+     * @param categorized_files Files to display in the review dialog.
+     */
     void show_results_dialog(const std::vector<CategorizedFile>& categorized_files);
+    /**
+     * @brief Shows a user-facing error dialog.
+     * @param message Error text to display.
+     */
     void show_error_dialog(const std::string& message);
+    /**
+     * @brief Appends a progress message to the active progress UI.
+     * @param message Progress text to report.
+     */
     void report_progress(const std::string& message);
+    /**
+     * @brief Requests cancellation of the currently running analysis, if any.
+     */
     void request_stop_analysis();
 
+    /**
+     * @brief Returns the currently selected folder path from the UI.
+     * @return Folder path as UTF-8 text.
+     */
     std::string get_folder_path() const;
+    /**
+     * @brief Returns whether the window is running in development mode.
+     * @return True when development-only features are enabled.
+     */
     bool is_development_mode() const { return development_mode_; }
 
 protected:
+    /**
+     * @brief Persists window state and handles shutdown when the window closes.
+     * @param event Qt close event being processed.
+     */
     void closeEvent(QCloseEvent* event) override;
 
 private:
diff --git a/app/include/MainAppUiBuilder.hpp b/app/include/MainAppUiBuilder.hpp
index 5c405b8..c33a261 100644
--- a/app/include/MainAppUiBuilder.hpp
+++ b/app/include/MainAppUiBuilder.hpp
@@ -7,9 +7,21 @@
 
 class MainApp;
 
+/**
+ * @brief Builds the MainApp widget tree, menus, and translation dependencies.
+ */
 class MainAppUiBuilder {
 public:
+    /**
+     * @brief Builds the main window UI for the provided application instance.
+     * @param app Main application window to populate.
+     */
     void build(MainApp& app);
+    /**
+     * @brief Collects the translator dependency bundle from the current UI state.
+     * @param app Main application window whose controls are referenced.
+     * @return Dependency bundle used by UiTranslator.
+     */
```

The excerpt is taken from the commit diff for `docs(ui): add Doxygen comments to main UI headers`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/include/MainAppUiBuilder.hpp`, `app/include/UiTranslator.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
