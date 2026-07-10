# 2025-11-20: refactor(app): improve

## Covered commits
- `bb666ef` `2025-11-20` `refactor(app): improve`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/LocalLLMClient.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `bb666ef`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -304,14 +304,29 @@ bool read_file_prefix(std::ifstream& file,
                       std::size_t requested_bytes,
                       std::size_t& bytes_read)
 {
-    if (requested_bytes == 0 || requested_bytes > buffer.size()) {
-        return false;
-    }
+    const auto compute_safe_request = [&](std::size_t& safe_request) -> bool {
+        if (requested_bytes == 0 || requested_bytes > buffer.size()) {
+            return false;
+        }
+        const auto max_streamsize = static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
+        const std::size_t clamped_request = std::min(requested_bytes, buffer.size());
+        safe_request = std::min(clamped_request, max_streamsize);
+        return safe_request > 0;
+    };
+
+    const auto validate_read_count = [&](std::streamsize read_count, std::size_t to_request) -> bool {
+        if (read_count <= 0) {
+            return false;
+        }
+        if (read_count > static_cast<std::streamsize>(to_request) ||
+            static_cast<std::size_t>(read_count) > buffer.size()) {
+            return false;
+        }
+        return true;
+    };
 
-    const auto max_streamsize = static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
-    const std::size_t clamped_request = std::min(requested_bytes, buffer.size());
-    const std::size_t safe_request = std::min(clamped_request, max_streamsize);
-    if (safe_request == 0) {
+    std::size_t safe_request = 0;
+    if (!compute_safe_request(safe_request)) {
         return false;
     }
 
@@ -322,10 +337,7 @@ bool read_file_prefix(std::ifstream& file,
     }
 
     const std::streamsize read_count = file.gcount();
-    if (read_count <= 0) {
-        return false;
-    }
-    if (read_count > to_request || static_cast<std::size_t>(read_count) > buffer.size()) {
+    if (!validate_read_count(read_count, safe_request)) {
         return false;
     }
```

The excerpt is taken from the commit diff for `refactor(app): improve`. The most relevant surfaces are `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
