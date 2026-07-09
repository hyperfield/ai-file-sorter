# 2025-06-18: chore: remove exclude-paths.txt (used in filter-repo)

## Covered commits
- `784f42e` `2025-06-18` `chore: remove exclude-paths.txt (used in filter-repo)`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LLMDownloader.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/LLMDownloader.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore: remove exclude-paths.txt (used in filter-repo)`.

Before this commit, the repository reflected the state immediately preceding `784f42e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LLMDownloader.cpp b/app/lib/LLMDownloader.cpp
--- a/app/lib/LLMDownloader.cpp
+++ b/app/lib/LLMDownloader.cpp
@@ -2,6 +2,7 @@
 #include "Utils.hpp"
 #include "DialogUtils.hpp"
 #include "ErrorMessages.hpp"
+#include <algorithm>
 #include <cstdlib>
 #include <curl/curl.h>
 #include <filesystem>
@@ -74,9 +75,9 @@ void LLMDownloader::parse_headers()
         
     CURLcode res = curl_easy_perform(curl);
 
-    double cl;
-    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl) == CURLE_OK && cl > 0) {
-        real_content_length = static_cast<curl_off_t>(cl);
+    curl_off_t cl;
+    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl) == CURLE_OK && cl > 0) {
+        real_content_length = cl;
     }
 
     curl_easy_cleanup(curl);
```

The excerpt is taken from the commit diff for `chore: remove exclude-paths.txt (used in filter-repo)`. The most relevant surfaces are `app/lib/LLMDownloader.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
