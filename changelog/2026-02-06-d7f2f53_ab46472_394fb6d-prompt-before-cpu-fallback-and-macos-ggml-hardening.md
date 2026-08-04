# 2026-02-06: Prompt-before-fallback for local LLMs and macOS ggml path hardening

## Covered commits
- `d7f2f53` `2026-02-06` `feat(local-llm): prompt before cpu fallback`
- `ab46472` `2026-02-06` `test(local-llm): cover gpu fallback decision`
- `394fb6d` `2026-02-06` `fix(macos): harden ggml backend path resolution`

## Motivation
Automatic CPU fallback is useful, but it should not silently hide GPU problems from users who care about performance. At the same time, macOS still needed harder guarantees that bundled ggml/llama runtime paths would beat conflicting system installs.

## What changed
These commits added a user prompt before falling back to CPU for local LLM failures, added tests around the decision path, and hardened ggml backend path resolution on macOS so the bundled runtime remained authoritative.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `d7f2f53`
```diff
diff --git a/app/include/LocalLLMClient.hpp b/app/include/LocalLLMClient.hpp
--- a/app/include/LocalLLMClient.hpp
+++ b/app/include/LocalLLMClient.hpp
@@ -25,8 +25,15 @@ public:
      * @param status Status event emitted by the client.
      */
     using StatusCallback = std::function<void(Status)>;
+    /**
+     * @brief Callback invoked when a GPU failure occurs to decide whether to retry on CPU.
+     * @param reason Short description of the failure cause.
+     * @return True to retry on CPU; false to abort.
+     */
+    using FallbackDecisionCallback = std::function<bool(const std::string& reason)>;
 
-    explicit LocalLLMClient(const std::string& model_path);
+    explicit LocalLLMClient(const std::string& model_path,
+                            FallbackDecisionCallback fallback_decision_callback = {});
     ~LocalLLMClient();
 
     std::string make_prompt(const std::string& file_name,
@@ -46,6 +53,11 @@ public:
      * @param callback Callback to invoke when status events occur.
      */
     void set_status_callback(StatusCallback callback);
+    /**
+     * @brief Registers a callback to decide whether GPU failures should fall back to CPU.
+     * @param callback Callback to invoke when a GPU failure is detected.
+     */
+    void set_fallback_decision_callback(FallbackDecisionCallback callback);
 
 private:
     void load_model_if_needed();
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `ab46472`
```diff
diff --git a/tests/unit/test_local_llm_backend.cpp b/tests/unit/test_local_llm_backend.cpp
--- a/tests/unit/test_local_llm_backend.cpp
+++ b/tests/unit/test_local_llm_backend.cpp
@@ -1,4 +1,5 @@
 #include <catch2/catch_test_macros.hpp>
+#include "LocalLLMClient.hpp"
 #include "LocalLLMTestAccess.hpp"
 #include "TestHooks.hpp"
 #include "TestHelpers.hpp"
@@ -113,4 +114,42 @@ TEST_CASE("Vulkan backend derives layer count from memory probe") {
     REQUIRE(params.n_gpu_layers > 0);
     REQUIRE(params.n_gpu_layers <= 48);
 }
+
+TEST_CASE("LocalLLMClient declines GPU fallback when callback returns false") {
+    TempModelFile model;
+    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
+    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "1");
+
+    bool called = false;
+    try {
+        LocalLLMClient client(model.path().string(),
+                              [&called](const std::string&) {
+                                  called = true;
+                                  return false;
+                              });
+        FAIL("Expected LocalLLMClient to throw when CPU fallback is declined");
+    } catch (const std::runtime_error& ex) {
+        REQUIRE(called);
+        REQUIRE(std::string(ex.what()).find("CPU fallback was declined") != std::string::npos);
+    }
+}
+
+TEST_CASE("LocalLLMClient retries on CPU when fallback is accepted") {
+    TempModelFile model;
+    EnvVarGuard backend("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
+    EnvVarGuard override_ngl("AI_FILE_SORTER_N_GPU_LAYERS", "1");
+
+    bool called = false;
+    try {
+        LocalLLMClient client(model.path().string(),
+                              [&called](const std::string&) {
+                                  called = true;
+                                  return true;
+                              });
+        FAIL("Expected LocalLLMClient to throw due to invalid model");
+    } catch (const std::runtime_error& ex) {
+        REQUIRE(called);
+        REQUIRE(std::string(ex.what()).find("Failed to load model") != std::string::npos);
+    }
+}
 #endif // GGML_USE_METAL
```

This second excerpt is included because `ab46472` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
