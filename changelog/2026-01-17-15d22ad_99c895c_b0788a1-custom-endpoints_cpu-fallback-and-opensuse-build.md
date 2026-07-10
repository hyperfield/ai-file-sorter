# 2026-01-17: Custom API endpoints, stronger CPU fallback, and OpenSUSE build fixes

## Covered commits
- `15d22ad` `2026-01-19` `feat(custom-llm-endpoints): add feature`
- `99c895c` `2026-01-17` `fix(local-llm-client): improve fallback on CPU (in case Vulkan VRAM is insufficient)`
- `b0788a1` `2026-01-17` `fix(opensuse-compile): make modifications to properly compile on OpenSUSE`
- `ea27631` `2026-01-17` `chore(llm-selection-dialog): update a label`

## Motivation
Users wanted more flexibility in how remote and local models were sourced. At the same time, GPU initialization failures on some systems still needed better CPU fallback behavior, and OpenSUSE users needed build fixes rather than distro-specific breakage.

## What changed
The grouped commits added custom API endpoint support for LLM configuration, strengthened the Vulkan-to-CPU fallback path, fixed OpenSUSE compilation issues, and cleaned up a label in the LLM selection dialog during the same pass.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `15d22ad`
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -5,6 +5,7 @@
 - Local 3B model download now defaults to Q4 for better GPU compatibility.
 - Legacy Local 3B Q8 is still selectable when an existing download is found.
 - LLM selection dialog now uses local file sizes for completed downloads when remote size metadata is unavailable.
+- Added custom OpenAI-compatible API endpoints (base URL + model + optional key) to the Select LLM dialog.
 - Bug fixes
 
 ## [1.5.0] - 2026-01-11
diff --git a/README.md b/README.md
index da2a4cc..f1b90f2 100644
--- a/README.md
+++ b/README.md
@@ -459,7 +459,8 @@ Visual LLM:
 Timeouts and logging:
 
 - `AI_FILE_SORTER_LOCAL_LLM_TIMEOUT` - seconds to wait for local LLM responses (default 60).
-- `AI_FILE_SORTER_REMOTE_LLM_TIMEOUT` - seconds to wait for remote LLM responses (default 10).
+- `AI_FILE_SORTER_REMOTE_LLM_TIMEOUT` - seconds to wait for OpenAI/Gemini responses (default 10).
+- `AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT` - seconds to wait for custom OpenAI-compatible API responses (default 60).
 - `AI_FILE_SORTER_LLAMA_LOGS` - enable verbose llama.cpp logs (`1`/`true`); also honors `LLAMA_CPP_DEBUG_LOGS`.
 
 Storage and updates:
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `99c895c`
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 1aa7c497c5e1929771c4fc3fc8e37c7157fa9bbd
+Subproject commit ee09828cb057460b369576410601a3a09279e23c
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
index 7281843..21d63ba 100644
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -1521,7 +1521,138 @@ std::string LocalLLMClient::generate_response(const std::string &prompt,
         logger->debug("Generating response with prompt length {} tokens target {}", prompt.size(), n_predict);
     }
 
-    auto* ctx = llama_init_from_model(model, ctx_params);
+    struct ContextAttempt {
+        int n_ctx;
+        int n_batch;
+    };
+
+    auto build_context_attempts = [](int n_ctx, int n_batch) {
+        std::vector<ContextAttempt> attempts;
+        auto add_attempt = [&](int ctx, int batch) {
+            ctx = std::max(ctx, 512);
+            batch = std::clamp(batch, 1, ctx);
+            if (ctx > n_ctx || batch > n_batch) {
+                return;
+            }
+            if (ctx == n_ctx && batch == n_batch) {
+                return;
+            }
+            for (const auto& existing : attempts) {
+                if (existing.n_ctx == ctx && existing.n_batch == batch) {
+                    return;
+                }
+            }
+            attempts.push_back({ctx, batch});
+        };
+
+        add_attempt(std::min(n_ctx, 2048), std::min(n_batch, 1024));
+        add_attempt(std::min(n_ctx, 1024), std::min(n_batch, 512));
+        add_attempt(std::min(n_ctx, 512), std::min(n_batch, 256));
+        return attempts;
+    };
+
+    auto try_init_context = [&](const llama_context_params& base_params,
+                                int n_ctx,
+                                int n_batch,
+                                llama_context_params& resolved_params) -> llama_context* {
+        llama_context_params attempt = base_params;
+        attempt.n_ctx = n_ctx;
+        attempt.n_batch = std::min(n_batch, n_ctx);
+        auto* ctx = llama_init_from_model(model, attempt);
+        if (ctx) {
+            resolved_params = attempt;
+        }
+        return ctx;
+    };
+
+    auto init_context_with_retries = [&](const llama_context_params& base_params,
+                                         bool cpu_attempt,
+                                         llama_context_params& resolved_params) -> llama_context* {
+        auto* ctx = try_init_context(base_params, base_params.n_ctx, base_params.n_batch, resolved_params);
+        if (ctx) {
+            return ctx;
+        }
+        if (logger) {
+            logger->warn("Failed to initialize llama context (n_ctx={}, n_batch={}); retrying with smaller buffers{}",
+                         base_params.n_ctx,
+                         base_params.n_batch,
+                         cpu_attempt ? " on CPU" : "");
+        }
+        for (const auto& attempt : build_context_attempts(base_params.n_ctx, base_params.n_batch)) {
+            if (logger) {
+                logger->warn("Retrying llama context init with n_ctx={}, n_batch={}{}",
+                             attempt.n_ctx,
+                             attempt.n_batch,
+                             cpu_attempt ? " on CPU" : "");
+            }
+            ctx = try_init_context(base_params, attempt.n_ctx, attempt.n_batch, resolved_params);
```

This second excerpt is included because `99c895c` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
