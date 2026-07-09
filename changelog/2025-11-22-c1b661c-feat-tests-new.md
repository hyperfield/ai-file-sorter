# 2025-11-22: feat(tests): new

## Covered commits
- `c1b661c` `2025-11-22` `feat(tests): new`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `tests/unit/test_categorization_dialog.cpp`
- `A` `tests/unit/test_custom_llm.cpp`
- `M` `tests/unit/test_whitelist_and_prompt.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `tests/unit/test_categorization_dialog.cpp`, `tests/unit/test_custom_llm.cpp`, `tests/unit/test_whitelist_and_prompt.cpp`. It changed the project from not having the capability described by `feat(tests): new` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `c1b661c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/tests/unit/test_categorization_dialog.cpp b/tests/unit/test_categorization_dialog.cpp
--- a/tests/unit/test_categorization_dialog.cpp
+++ b/tests/unit/test_categorization_dialog.cpp
@@ -2,6 +2,8 @@
 #include "CategorizationDialog.hpp"
 #include "TestHooks.hpp"
 #include "TestHelpers.hpp"
+#include <QTableView>
+#include <QStandardItemModel>
 #include <filesystem>
 #include <fstream>
 
@@ -67,6 +69,47 @@ TEST_CASE("CategorizationDialog uses subcategory toggle when moving files") {
     }
 }
 
+#ifndef _WIN32
+TEST_CASE("CategorizationDialog supports sorting by columns") {
+    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
+    QtAppContext qt_context;
+
+    CategorizedFile alpha;
+    alpha.file_path = "/tmp";
+    alpha.file_name = "b.txt";
+    alpha.type = FileType::File;
+    alpha.category = "Alpha";
+    alpha.subcategory = "One";
+
+    CategorizedFile beta;
+    beta.file_path = "/tmp";
+    beta.file_name = "a.txt";
+    beta.type = FileType::File;
+    beta.category = "Beta";
+    beta.subcategory = "Two";
+
+    CategorizationDialog dialog(nullptr, true);
+    dialog.test_set_entries({alpha, beta});
+
+    auto* table = dialog.findChild<QTableView*>();
+    REQUIRE(table != nullptr);
+    auto* model = qobject_cast<QStandardItemModel*>(table->model());
+    REQUIRE(model != nullptr);
+
+    SECTION("Sorts by file name ascending") {
+        table->sortByColumn(1, Qt::AscendingOrder); // file name column
+        REQUIRE(model->item(0, 1)->text() == QStringLiteral("a.txt"));
+        REQUIRE(model->item(1, 1)->text() == QStringLiteral("b.txt"));
+    }
+
+    SECTION("Sorts by category descending") {
+        table->sortByColumn(3, Qt::DescendingOrder); // category column
+        REQUIRE(model->item(0, 3)->text() == QStringLiteral("Beta"));
+        REQUIRE(model->item(1, 3)->text() == QStringLiteral("Alpha"));
+    }
+}
+#endif
+
 #ifndef _WIN32
 TEST_CASE("CategorizationDialog undo restores moved files") {
     EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
```

The excerpt is taken from the commit diff for `feat(tests): new`. The most relevant surfaces are `tests/unit/test_categorization_dialog.cpp`, `tests/unit/test_custom_llm.cpp`, `tests/unit/test_whitelist_and_prompt.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
