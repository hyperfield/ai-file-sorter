# 2025-12-05: Vulkan enforcement, platform stability tweaks, and downloader range-error handling

## Covered commits
- `a0202c4` `2025-12-05` `fix(app): vulkan enforcement for more reliability`
- `9222704` `2025-12-05` `chore(app): adjustments for vulkan on linux`
- `579ed60` `2025-12-05` `fix(app): adjustments for windows`
- `716e036` `2025-12-05` `fix(app): remove splash screen`
- `5394320` `2025-12-06` `fix(app): tweaks for linux`
- `49f74d8` `2025-12-08` `fix(llm-downloader): improve handling of CURLE_HTTP_RANGE_ERROR`
- `0613407` `2025-12-08` `feat(tests): update, add new`

## Motivation
The December reliability pass focused on two classes of issues: runtime selection/launch stability on Linux and Windows, and robustness of long model downloads. Vulkan support existed, but the product still needed stronger enforcement and safer platform-specific behavior around it.

## What changed
These commits refined Vulkan handling on Linux, removed the splash screen where it caused friction, adjusted Windows-specific app behavior, improved download recovery for HTTP range errors, and expanded tests to lock in the downloader changes.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `a0202c4`
```diff
diff --git a/app/build_windows.ps1 b/app/build_windows.ps1
--- a/app/build_windows.ps1
+++ b/app/build_windows.ps1
@@ -296,6 +296,7 @@ foreach ($dllName in $mingwRuntimeNames) {
         $candidate = Join-Path $path $dllName
         if (Test-Path $candidate) {
             Copy-Item $candidate -Destination $destWocuda -Force
+            Copy-Item $candidate -Destination $outputDir -Force
             $found = $true
             break
         }
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
index a5bdfb3..14b4603 100644
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -1328,21 +1328,31 @@ llama_model_params build_model_params_for_path(const std::string& model_path,
         return model_params;
     }
 
+    const bool prefer_vulkan = (backend_pref == PreferredBackend::Vulkan) ||
+                               (backend_pref == PreferredBackend::Auto);
+
+    if (prefer_vulkan) {
+        // Vulkan is the primary backend; keep CUDA disabled and steer llama.cpp to Vulkan.
+        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
+        set_env_var("LLAMA_ARG_DEVICE", "vulkan");
+        apply_vulkan_backend(model_path, model_params, logger);
+        return model_params;
+    }
+
+    // CUDA requested explicitly.
+    if (handle_cuda_forced_off(cuda_forced_off, backend_pref, model_params, logger)) {
+        return model_params;
+    }
+
     const bool cudaConfigured = configure_cuda_backend(model_path, model_params, logger);
     if (!cudaConfigured) {
         if (logger) {
-            if (backend_pref == PreferredBackend::Cuda) {
-                logger->warn("CUDA backend explicitly requested but unavailable; attempting Vulkan fallback.");
-            } else {
-                logger->warn("CUDA backend unavailable; attempting Vulkan fallback.");
-            }
+            logger->warn("CUDA backend explicitly requested but unavailable; attempting Vulkan fallback.");
         }
+        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
+        set_env_var("LLAMA_ARG_DEVICE", "vulkan");
         apply_vulkan_backend(model_path, model_params, logger);
         return model_params;
-        if (logger) {
-            logger->warn("Vulkan fallback unavailable; using CPU backend.");
-        }
-        return model_params;
     }
 #endif
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `9222704`
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -22,8 +22,8 @@ ifeq ($(UNAME), Linux)
 
     # --- Other dependencies ---
 	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lggml-base -lX11 -pthread
-	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled/cpu/bin'
-	LDFLAGS += -Wl,-rpath-link=./lib/precompiled/cpu/bin
+	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled/cpu/bin' -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
+	LDFLAGS += -Wl,-rpath-link=./lib/precompiled/cpu/bin -Wl,-rpath-link=./lib/precompiled
 
 else ifeq ($(UNAME), Darwin)
     IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
@@ -100,7 +100,7 @@ endif
 INCLUDE_DIRS = -I./include -I./include/llama
 LIB_DIRS =
 ifeq ($(UNAME), Linux)
-LIB_DIRS += -L./lib/precompiled/cpu/bin
+LIB_DIRS += -L./lib/precompiled/cpu/bin -L./lib/precompiled
 else
 LIB_DIRS += -L./lib/precompiled
 endif
```

This second excerpt is included because `9222704` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
