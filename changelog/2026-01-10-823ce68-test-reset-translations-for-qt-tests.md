# 2026-01-10: test: reset translations for Qt tests

## Covered commits
- `823ce68` `2026-01-10` `test: reset translations for Qt tests`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `tests/unit/TestHelpers.hpp`

## What changed from what, why, and how
The commit updated test-related files in `tests/unit/TestHelpers.hpp`. It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `823ce68`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/tests/unit/TestHelpers.hpp b/tests/unit/TestHelpers.hpp
--- a/tests/unit/TestHelpers.hpp
+++ b/tests/unit/TestHelpers.hpp
@@ -12,6 +12,8 @@
 #include <vector>
 #include <cstring>
 #include <QApplication>
+#include "TranslationManager.hpp"
+#include "Language.hpp"
 
 inline std::string make_unique_token(std::string_view prefix) {
     static std::atomic<uint64_t> counter{0};
@@ -144,5 +146,7 @@ public:
             static QApplication* app = new QApplication(argc, argv);
             Q_UNUSED(app);
         }
+        TranslationManager::instance().initialize(qApp);
+        TranslationManager::instance().set_language(Language::English);
     }
 };
```

The excerpt is taken from the commit diff for `test: reset translations for Qt tests`. The most relevant surfaces are `tests/unit/TestHelpers.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
