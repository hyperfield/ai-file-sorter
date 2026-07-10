# 2025-11-06: feat(categorization): imrpove category consistency algorithm, add --developer mode

## Covered commits
- `932e3ab` `2025-11-06` `feat(categorization): imrpove category consistency algorithm, add --developer mode`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationService.hpp`
- `M` `app/include/ConsistencyPassService.hpp`
- `M` `app/include/DatabaseManager.hpp`
- `M` `app/include/ILLMClient.hpp`
- `M` `app/include/LLMClient.hpp`
- `M` `app/include/LocalLLMClient.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/MainAppUiBuilder.hpp`
- `M` `app/include/Settings.hpp`
- `M` `app/lib/CategorizationService.cpp`
- `M` `app/lib/ConsistencyPassService.cpp`
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/LLMClient.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MainAppUiBuilder.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/TranslationManager.cpp`
- `M` `app/main.cpp`
- `M` `app/startapp_linux.cpp`
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/include/CategorizationService.hpp`, `app/include/ConsistencyPassService.hpp`, `app/include/DatabaseManager.hpp`, `app/include/ILLMClient.hpp`, `app/include/LLMClient.hpp`, `app/include/LocalLLMClient.hpp`, `app/include/MainApp.hpp`, `app/include/MainAppUiBuilder.hpp`, and 13 more file(s). It changed the project from not having the capability described by `feat(categorization): imrpove category consistency algorithm, add --developer mode` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `932e3ab`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationService.hpp b/app/include/CategorizationService.hpp
--- a/app/include/CategorizationService.hpp
+++ b/app/include/CategorizationService.hpp
@@ -5,10 +5,12 @@
 #include "DatabaseManager.hpp"
 
 #include <atomic>
+#include <deque>
 #include <functional>
 #include <memory>
 #include <optional>
 #include <string>
+#include <unordered_map>
 #include <vector>
 
 class Settings;
@@ -39,13 +41,18 @@ public:
         std::function<std::unique_ptr<ILLMClient>()> llm_factory) const;
 
 private:
+    using CategoryPair = std::pair<std::string, std::string>;
+    using HintHistory = std::deque<CategoryPair>;
+    using SessionHistoryMap = std::unordered_map<std::string, HintHistory>;
+
     DatabaseManager::ResolvedCategory categorize_with_cache(
         ILLMClient& llm,
         bool is_local_llm,
         const std::string& item_name,
         const std::string& item_path,
         FileType file_type,
-        const ProgressCallback& progress_callback) const;
+        const ProgressCallback& progress_callback,
+        const std::string& consistency_context) const;
 
     std::optional<CategorizedFile> categorize_single_entry(
         ILLMClient& llm,
```

The excerpt is taken from the commit diff for `feat(categorization): imrpove category consistency algorithm, add --developer mode`. The most relevant surfaces are `app/include/CategorizationService.hpp`, `app/include/ConsistencyPassService.hpp`, `app/include/DatabaseManager.hpp`, `app/include/ILLMClient.hpp`, `app/include/LLMClient.hpp`, and 16 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
