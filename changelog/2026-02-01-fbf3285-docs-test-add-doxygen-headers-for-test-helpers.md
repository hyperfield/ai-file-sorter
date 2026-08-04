# 2026-02-01: docs(test): add Doxygen headers for test helpers

## Covered commits
- `fbf3285` `2026-02-01` `docs(test): add Doxygen headers for test helpers`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainAppTestAccess.hpp`
- `M` `tests/unit/TestHelpers.hpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/include/MainAppTestAccess.hpp`, `tests/unit/TestHelpers.hpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `fbf3285`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainAppTestAccess.hpp b/app/include/MainAppTestAccess.hpp
--- a/app/include/MainAppTestAccess.hpp
+++ b/app/include/MainAppTestAccess.hpp
@@ -1,3 +1,7 @@
+/**
+ * @file MainAppTestAccess.hpp
+ * @brief Test-only accessors and helpers for MainApp.
+ */
 #pragma once
 
 #ifdef AI_FILE_SORTER_TEST_BUILD
@@ -13,15 +17,99 @@
 class MainApp;
 class Settings;
 
+/**
+ * @brief Provides test access to MainApp internals and helpers.
+ */
 class MainAppTestAccess {
 public:
-    enum class SimulatedSupportResult { Support, NotSure, CannotDonate };
+    /**
+     * @brief Simulated responses for the support prompt flow.
+     */
+    enum class SimulatedSupportResult {
+        /// User agreed to support.
+        Support,
+        /// User is unsure.
+        NotSure,
+        /// User cannot donate.
+        CannotDonate
+    };
+
+    /**
+     * @brief Read the analyze button label text.
+     * @param app MainApp instance.
+     * @return Current analyze button text.
+     */
     static QString analyze_button_text(const MainApp& app);
+    /**
+     * @brief Read the folder/path label text.
+     * @param app MainApp instance.
+     * @return Current path label text.
+     */
     static QString path_label_text(const MainApp& app);
+    /**
+     * @brief Access the \"Categorize files\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
+    static QCheckBox* categorize_files_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Analyze picture files\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
     static QCheckBox* analyze_images_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Process picture files only\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
     static QCheckBox* process_images_only_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Offer to rename picture files\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
     static QCheckBox* offer_rename_images_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Do not categorize picture files\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
     static QCheckBox* rename_images_only_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Analyze document files\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
+    static QCheckBox* analyze_documents_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Process document files only\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
+    static QCheckBox* process_documents_only_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Do not categorize document files\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
+    static QCheckBox* rename_documents_only_checkbox(MainApp& app);
+    /**
+     * @brief Split file entries into image/document/other buckets for analysis.
+     * @param files Input entries to split.
+     * @param analyze_images Whether to analyze images by content.
+     * @param analyze_documents Whether to analyze documents by content.
+     * @param process_images_only Whether only images should be processed.
+     * @param process_documents_only Whether only documents should be processed.
```

The excerpt is taken from the commit diff for `docs(test): add Doxygen headers for test helpers`. The most relevant surfaces are `app/include/MainAppTestAccess.hpp`, `tests/unit/TestHelpers.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
