# 2025-11-11: chore(main): refactor for more readable code

## Covered commits
- `d52ffc6` `2025-11-11` `chore(main): refactor for more readable code`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LLMClient.cpp`
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/LLMClient.cpp`, `app/lib/LocalLLMClient.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `d52ffc6`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LLMClient.cpp b/app/lib/LLMClient.cpp
--- a/app/lib/LLMClient.cpp
+++ b/app/lib/LLMClient.cpp
@@ -14,6 +14,7 @@
 
 #include <iostream>
 #include <sstream>
+#include <string>
 
 namespace {
 std::string escape_json(const std::string& input) {
@@ -32,75 +33,79 @@ std::string escape_json(const std::string& input) {
     }
     return out;
 }
-}
-
-
-// Helper function to write the response from curl into a string
-static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response)
-{
-    size_t totalSize = size * nmemb;
-    response->append(static_cast<const char*>(contents), totalSize);
-    return totalSize;
-}
-
-
-LLMClient::LLMClient(const std::string &api_key) : api_key(api_key)
-{}
 
+struct CurlRequest {
+    CURL* handle{nullptr};
+    curl_slist* headers{nullptr};
 
-LLMClient::~LLMClient() = default;
-
+    CurlRequest() = default;
+    CurlRequest(const CurlRequest&) = delete;
+    CurlRequest& operator=(const CurlRequest&) = delete;
 
-void LLMClient::set_prompt_logging_enabled(bool enabled)
-{
-    prompt_logging_enabled = enabled;
-}
+    CurlRequest(CurlRequest&& other) noexcept
+        : handle(other.handle),
+          headers(other.headers)
+    {
+        other.handle = nullptr;
+        other.headers = nullptr;
+    }
 
+    CurlRequest& operator=(CurlRequest&& other) noexcept
+    {
+        if (this != &other) {
+            cleanup();
+            handle = other.handle;
+            headers = other.headers;
+            other.handle = nullptr;
+            other.headers = nullptr;
+        }
+        return *this;
+    }
 
-std::string LLMClient::send_api_request(std::string json_payload) {
-    CURL *curl;
-    CURLcode res;
-    std::string response_string;
-    std::string api_url = "https://api.openai.com/v1/chat/completions";
-    auto logger = Logger::get_logger("core_logger");
+    ~CurlRequest() {
+        cleanup();
+    }
 
-    if (logger) {
-        logger->debug("Dispatching remote LLM request to {}", api_url);
+private:
+    void cleanup()
+    {
+        if (handle) {
+            curl_easy_cleanup(handle);
+            handle = nullptr;
+        }
+        if (headers) {
+            curl_slist_free_all(headers);
+            headers = nullptr;
+        }
     }
+};
 
-    curl = curl_easy_init();
-    if (!curl) {
+CurlRequest create_curl_request(const std::shared_ptr<spdlog::logger>& logger)
+{
+    CurlRequest request;
+    request.handle = curl_easy_init();
+    if (!request.handle) {
         if (logger) {
             logger->critical("Failed to initialize cURL handle for remote request");
         }
```

The excerpt is taken from the commit diff for `chore(main): refactor for more readable code`. The most relevant surfaces are `app/lib/LLMClient.cpp`, `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
