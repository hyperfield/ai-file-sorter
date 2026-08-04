# 2025-11-20: refactor(app): improve

## Covered commits
- `6a55821` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CategorizationService.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/lib/CategorizationService.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/include/CategorizationService.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/lib/CategorizationService.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/LocalLLMClient.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `6a55821`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CategorizationService.hpp b/app/include/CategorizationService.hpp
--- a/app/include/CategorizationService.hpp
+++ b/app/include/CategorizationService.hpp
@@ -6,6 +6,7 @@
 
 #include <atomic>
 #include <deque>
+#include <future>
 #include <functional>
 #include <memory>
 #include <optional>
@@ -91,6 +92,12 @@ private:
         FileType file_type,
         bool is_local_llm,
         const std::string& consistency_context) const;
+    int resolve_llm_timeout(bool is_local_llm) const;
+    std::future<std::string> start_llm_future(ILLMClient& llm,
+                                              const std::string& item_name,
+                                              const std::string& item_path,
+                                              FileType file_type,
+                                              const std::string& consistency_context) const;
 
     std::string build_whitelist_context() const;
     std::string build_category_language_context() const;
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/include/CategorizationService.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/lib/CategorizationService.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
