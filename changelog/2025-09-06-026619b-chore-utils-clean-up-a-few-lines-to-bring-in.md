# 2025-09-06: chore(utils): clean up a few lines to bring in accordance with linter

## Covered commits
- `026619b` `2025-09-06` `chore(utils): clean up a few lines to bring in accordance with linter`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/Utils.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `026619b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -110,7 +110,8 @@ bool Utils::is_valid_directory(const char *path)
 std::vector<unsigned char> Utils::hex_to_vector(const std::string& hex) {
     std::vector<unsigned char> data;
     for (size_t i = 0; i < hex.length(); i += 2) {
-        unsigned char byte = static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16));
+        unsigned char byte = static_cast<unsigned char>(
+            std::stoi(hex.substr(i, 2), nullptr, 16));
         data.push_back(byte);
     }
     return data;
@@ -166,7 +167,8 @@ std::string Utils::format_size(curl_off_t bytes)
 {
     char buffer[64];
     if (bytes >= (1LL << 30))
-        snprintf(buffer, sizeof(buffer), "%.2f GB", bytes / (double)(1LL << 30));
+        snprintf(buffer, sizeof(buffer), "%.2f GB",
+                 bytes / (double)(1LL << 30));
     else if (bytes >= (1LL << 20))
         snprintf(buffer, sizeof(buffer), "%.2f MB", bytes / (double)(1LL << 20));
     else if (bytes >= (1LL << 10))
```

The excerpt is taken from the commit diff for `chore(utils): clean up a few lines to bring in accordance with linter`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
