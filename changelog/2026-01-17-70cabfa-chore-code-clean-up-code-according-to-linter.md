# 2026-01-17: chore(code): clean up code according to linter

## Covered commits
- `70cabfa` `2026-01-17` `chore(code): clean up code according to linter`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Settings.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/Settings.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `70cabfa`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Settings.cpp b/app/lib/Settings.cpp
--- a/app/lib/Settings.cpp
+++ b/app/lib/Settings.cpp
@@ -192,12 +192,14 @@ Settings::Settings()
     }
 
     if (default_sort_folder.empty()) {
-        default_sort_folder = Utils::path_to_utf8(std::filesystem::current_path());
+        default_sort_folder = Utils::path_to_utf8(
+            std::filesystem::current_path());
     }
 
     sort_folder = default_sort_folder;
 
-    // Default language follows system locale on first run (before any config file exists).
+    // Default language follows system locale on first run
+    // (before any config file exists)
     language = system_default_language();
     category_language = CategoryLanguage::English;
     analyze_images_by_content = visual_llm_files_available();
@@ -206,7 +208,8 @@ Settings::Settings()
 LLMChoice Settings::parse_llm_choice() const
 {
     const std::string value = config.getValue("Settings", "LLMChoice", "Unset");
-    if (value == "Remote" || value == "Remote_OpenAI") return LLMChoice::Remote_OpenAI;
+    if (value == "Remote" || value == "Remote_OpenAI")
+        return LLMChoice::Remote_OpenAI;
     if (value == "Remote_Gemini") return LLMChoice::Remote_Gemini;
     if (value == "Local_3b") return LLMChoice::Local_3b;
     if (value == "Local_7b") return LLMChoice::Local_7b;
```

The excerpt is taken from the commit diff for `chore(code): clean up code according to linter`. The most relevant surfaces are `app/lib/Settings.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
