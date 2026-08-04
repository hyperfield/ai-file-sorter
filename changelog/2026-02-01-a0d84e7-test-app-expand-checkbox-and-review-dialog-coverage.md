# 2026-02-01: test(app): expand checkbox and review dialog coverage

## Covered commits
- `a0d84e7` `2026-02-01` `test(app): expand checkbox and review dialog coverage`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/CMakeLists.txt`
- `A` `tests/unit/test_checkbox_matrix.cpp`
- `A` `tests/unit/test_cli_reporter.cpp`
- `M` `tests/unit/test_main_app_image_options.cpp`
- `A` `tests/unit/test_review_dialog_rename_gate.cpp`

## What changed from what, why, and how
The commit updated test-related files in `app/CMakeLists.txt`, `tests/unit/test_checkbox_matrix.cpp`, `tests/unit/test_cli_reporter.cpp`, `tests/unit/test_main_app_image_options.cpp`, `tests/unit/test_review_dialog_rename_gate.cpp`. It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `a0d84e7`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -516,6 +516,9 @@ if(AI_FILE_SORTER_BUILD_TESTS)
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_settings_image_options.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_ui_translator.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_categorization_dialog.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_checkbox_matrix.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_review_dialog_rename_gate.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_cli_reporter.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_support_prompt.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_whitelist_and_prompt.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_database_manager_rename_only.cpp"
diff --git a/tests/unit/test_checkbox_matrix.cpp b/tests/unit/test_checkbox_matrix.cpp
new file mode 100644
index 0000000..b97ffd0
--- /dev/null
+++ b/tests/unit/test_checkbox_matrix.cpp
@@ -0,0 +1,202 @@
+#include <catch2/catch_test_macros.hpp>
+
+#include "DocumentTextAnalyzer.hpp"
+#include "LlavaImageAnalyzer.hpp"
+#include "MainAppTestAccess.hpp"
+#include "Types.hpp"
+
+#include <algorithm>
+#include <filesystem>
+#include <iostream>
+#include <sstream>
+#include <string>
+#include <unordered_set>
+#include <vector>
+
+namespace {
+
+struct Combo {
+    bool analyze_images;
+    bool analyze_documents;
+    bool process_images_only;
+    bool process_documents_only;
+    bool rename_images_only;
+    bool rename_documents_only;
+    bool categorize_files;
+};
+
+enum class Bucket {
+    None,
+    Image,
+    Document,
+    Other
+};
+
+std::string combo_label(const Combo& combo) {
+    std::ostringstream out;
+    out << "AI=" << combo.analyze_images
+        << " AD=" << combo.analyze_documents
+        << " PI=" << combo.process_images_only
+        << " PD=" << combo.process_documents_only
+        << " RI=" << combo.rename_images_only
+        << " RD=" << combo.rename_documents_only
+        << " CF=" << combo.categorize_files;
+    return out.str();
+}
+
+bool contains_name(const std::vector<FileEntry>& entries, const std::string& name) {
+    return std::any_of(entries.begin(), entries.end(), [&name](const FileEntry& entry) {
+        return entry.file_name == name;
+    });
+}
+
+Bucket expected_bucket(const FileEntry& entry,
+                       const Combo& combo,
+                       const std::unordered_set<std::string>& renamed_files) {
+    const bool restrict_types = combo.process_images_only || combo.process_documents_only;
+    const bool allow_images = !restrict_types || combo.process_images_only;
+    const bool allow_documents = !restrict_types || combo.process_documents_only;
+    const bool allow_other_files = combo.categorize_files && !restrict_types;
+
+    if (entry.type == FileType::Directory) {
+        return restrict_types ? Bucket::None : Bucket::Other;
+    }
+
+    const bool is_image = LlavaImageAnalyzer::is_supported_image(entry.full_path);
+    const bool is_document = DocumentTextAnalyzer::is_supported_document(entry.full_path);
+
+    if (is_image) {
+        if (!allow_images) {
+            return Bucket::None;
+        }
+        if (combo.analyze_images) {
+            const bool already_renamed = renamed_files.contains(entry.file_name);
+            if (already_renamed) {
+                return combo.rename_images_only ? Bucket::None : Bucket::Other;
+            }
+            return Bucket::Image;
+        }
+        return allow_other_files ? Bucket::Other : Bucket::None;
+    }
+
```

The excerpt is taken from the commit diff for `test(app): expand checkbox and review dialog coverage`. The most relevant surfaces are `app/CMakeLists.txt`, `tests/unit/test_checkbox_matrix.cpp`, `tests/unit/test_cli_reporter.cpp`, `tests/unit/test_main_app_image_options.cpp`, `tests/unit/test_review_dialog_rename_gate.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
