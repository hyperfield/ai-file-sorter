# 2026-03-08: fix(support): use fixed follow-up prompt thresholds

## Covered commits
- `10a2cc5` `2026-03-08` `fix(support): use fixed follow-up prompt thresholds`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/MainApp.hpp`
- `M` `app/include/MainAppTestAccess.hpp`
- `M` `app/lib/MainApp.cpp`
- `M` `tests/unit/test_support_prompt.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/include/MainApp.hpp`, `app/include/MainAppTestAccess.hpp`, `app/lib/MainApp.cpp`, `tests/unit/test_support_prompt.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(support): use fixed follow-up prompt thresholds`.

Before this commit, the repository reflected the state immediately preceding `10a2cc5`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -68,7 +68,7 @@ public:
     /**
      * @brief Outcome returned from the optional support prompt flow.
      */
-    enum class SupportPromptResult { Support, NotSure, CannotDonate };
+    enum class SupportPromptResult { Support, NotSure };
     /**
      * @brief Constructs the main application window.
      * @param settings Persistent settings store used by the window.
diff --git a/app/include/MainAppTestAccess.hpp b/app/include/MainAppTestAccess.hpp
index df3c810..7f78330 100644
--- a/app/include/MainAppTestAccess.hpp
+++ b/app/include/MainAppTestAccess.hpp
@@ -30,9 +30,7 @@ public:
         /// User entered a valid donation code and hid the prompt.
         Support,
         /// User is unsure.
-        NotSure,
-        /// User cannot donate.
-        CannotDonate
+        NotSure
     };
 
     /**
```

The excerpt is taken from the commit diff for `fix(support): use fixed follow-up prompt thresholds`. The most relevant surfaces are `app/include/MainApp.hpp`, `app/include/MainAppTestAccess.hpp`, `app/lib/MainApp.cpp`, `tests/unit/test_support_prompt.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
