# 2025-11-20: refactor(app): improve

## Covered commits
- `25c89d8` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/EmbeddedEnv.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/UiTranslator.hpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/UiTranslator.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/EmbeddedEnv.hpp`, `app/include/MainApp.hpp`, `app/include/UiTranslator.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `app/lib/UiTranslator.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `25c89d8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/EmbeddedEnv.hpp b/app/include/EmbeddedEnv.hpp
--- a/app/include/EmbeddedEnv.hpp
+++ b/app/include/EmbeddedEnv.hpp
@@ -16,4 +16,4 @@ private:
     std::string trim(const std::string& str);
 };
 
-#endif
\ No newline at end of file
+#endif
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
index 8ab94fd..92960ea 100644
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -90,6 +90,9 @@ private:
     void restore_file_explorer_visibility();
     void restore_development_preferences();
     void connect_signals();
+    void connect_folder_contents_signals();
+    void connect_checkbox_signals();
+    void connect_whitelist_signals();
     void connect_edit_actions();
     void start_updater();
     void set_app_icon();
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/include/EmbeddedEnv.hpp`, `app/include/MainApp.hpp`, `app/include/UiTranslator.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, and 1 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
