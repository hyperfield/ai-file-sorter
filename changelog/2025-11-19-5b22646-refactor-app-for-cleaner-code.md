# 2025-11-19: refactor(app): for cleaner code

## Covered commits
- `5b22646` `2025-11-19` `refactor(app): for cleaner code`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/Settings.hpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `tests/unit/test_local_llm_backend.cpp`
- `M` `tests/unit/test_support_prompt.cpp`
- `M` `tests/unit/test_ui_translator.cpp`
- `M` `tests/unit/test_whitelist_and_prompt.cpp`

## What changed from what, why, and how
The commit updated test-related files in `app/include/Settings.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Settings.cpp`, `tests/unit/test_local_llm_backend.cpp`, `tests/unit/test_support_prompt.cpp`, `tests/unit/test_ui_translator.cpp`, `tests/unit/test_whitelist_and_prompt.cpp`. It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `5b22646`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/Settings.hpp b/app/include/Settings.hpp
--- a/app/include/Settings.hpp
+++ b/app/include/Settings.hpp
@@ -8,6 +8,7 @@
 #include <string>
 #include <filesystem>
 #include <vector>
+#include <functional>
 
 
 class Settings
@@ -75,6 +76,17 @@ public:
     void set_allowed_subcategories(std::vector<std::string> values);
 
 private:
+    LLMChoice parse_llm_choice() const;
+    void load_basic_settings(const std::function<bool(const char*, bool)>& load_bool,
+                             const std::function<int(const char*, int, int)>& load_int);
+    void load_whitelist_settings(const std::function<bool(const char*, bool)>& load_bool);
+    void load_custom_llm_settings();
+    void log_loaded_settings() const;
+
+    void save_core_settings();
+    void save_whitelist_settings();
+    void save_custom_llms();
+
     std::string config_path;
     std::filesystem::path config_dir;
     IniConfig config;
```

The excerpt is taken from the commit diff for `refactor(app): for cleaner code`. The most relevant surfaces are `app/include/Settings.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Settings.cpp`, `tests/unit/test_local_llm_backend.cpp`, `tests/unit/test_support_prompt.cpp`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
