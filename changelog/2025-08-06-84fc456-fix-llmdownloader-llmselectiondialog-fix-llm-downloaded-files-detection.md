# 2025-08-06: fix(LLMDownloader, LLMSelectionDialog): fix LLM downloaded files detection issue on Windows

## Covered commits
- `84fc456` `2025-08-06` `fix(LLMDownloader, LLMSelectionDialog): fix LLM downloaded files detection issue on Windows`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(LLMDownloader, LLMSelectionDialog): fix LLM downloaded files detection issue on Windows`.

Before this commit, the repository reflected the state immediately preceding `84fc456`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LLMDownloader.cpp b/app/lib/LLMDownloader.cpp
--- a/app/lib/LLMDownloader.cpp
+++ b/app/lib/LLMDownloader.cpp
@@ -20,15 +20,13 @@ LLMDownloader::LLMDownloader(const std::string& download_url)
 }
 
 
-void LLMDownloader::set_download_destination()
-{
+void LLMDownloader::set_download_destination() {
     std::filesystem::create_directories(destination_dir);
     download_destination = Utils::make_default_path_to_file_from_download_url(url);
 }
 
 
-void LLMDownloader::init_if_needed()
-{
+void LLMDownloader::init_if_needed() {
     if (initialized) return;
 
     if (!Utils::is_network_available()) {
@@ -318,15 +316,26 @@ bool LLMDownloader::is_download_resumable() const
 
 bool LLMDownloader::is_download_complete() const
 {
-    FILE* fp = fopen(download_destination.c_str(), "rb");
-    if (!fp) return false;
+    try {
+        auto file_size = std::filesystem::file_size(download_destination);
+        return static_cast<std::int64_t>(file_size) >= real_content_length;
+    } catch (const std::filesystem::filesystem_error&) {
+        return false;
+    }
+}
 
-    fseek(fp, 0, SEEK_END);
-    long size = ftell(fp);
-    fclose(fp);
 
-    return size >= real_content_length;
-}
+// bool LLMDownloader::is_download_complete() const
+// {
+//     FILE* fp = fopen(download_destination.c_str(), "rb");
+//     if (!fp) return false;
+
+//     fseek(fp, 0, SEEK_END);
+//     long size = ftell(fp);
+//     fclose(fp);
+
+//     return size >= real_content_length;
+// }
 
 
 long long LLMDownloader::get_real_content_length() const
```

The excerpt is taken from the commit diff for `fix(LLMDownloader, LLMSelectionDialog): fix LLM downloaded files detection issue on Windows`. The most relevant surfaces are `app/lib/LLMDownloader.cpp`, `app/lib/LLMSelectionDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
