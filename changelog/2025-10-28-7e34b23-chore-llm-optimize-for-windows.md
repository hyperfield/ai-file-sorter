# 2025-10-28: chore(LLM): optimize for Windows

## Covered commits
- `7e34b23` `2025-10-28` `chore(LLM): optimize for Windows`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LLMClient.cpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/Updater.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/external/llama.cpp`, `app/lib/LLMClient.cpp`, `app/lib/LLMDownloader.cpp`, `app/lib/Settings.cpp`, `app/lib/Updater.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(LLM): optimize for Windows`.

Before this commit, the repository reflected the state immediately preceding `7e34b23`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
+Subproject commit 3cfa9c3f125763305b4226bc032f1954f08990dc
diff --git a/app/lib/LLMClient.cpp b/app/lib/LLMClient.cpp
index c2804fc..24f4947 100644
--- a/app/lib/LLMClient.cpp
+++ b/app/lib/LLMClient.cpp
@@ -71,8 +71,13 @@ std::string LLMClient::send_api_request(std::string json_payload) {
     }
 
     #ifdef _WIN32
-        std::string cert_path = std::filesystem::current_path().string() + "\\certs\\cacert.pem";
-        curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.c_str());
+        try {
+            const auto cert_path = Utils::ensure_ca_bundle();
+            curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
+        } catch (const std::exception& ex) {
+            curl_easy_cleanup(curl);
+            throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
+        }
     #endif
     curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
     curl_easy_setopt(curl, CURLOPT_POST, 1L);
```

The excerpt is taken from the commit diff for `chore(LLM): optimize for Windows`. The most relevant surfaces are `app/include/external/llama.cpp`, `app/lib/LLMClient.cpp`, `app/lib/LLMDownloader.cpp`, `app/lib/Settings.cpp`, `app/lib/Updater.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
