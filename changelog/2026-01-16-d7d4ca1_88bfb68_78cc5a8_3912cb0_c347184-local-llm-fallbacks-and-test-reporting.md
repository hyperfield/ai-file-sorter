# 2026-01-16: Local-LLM fallback behavior, visual rename defaults, and test-output reporting

## Covered commits
- `d7d4ca1` `2026-01-16` `fix(llama-cpp-fallback): improve fallback on cpu in case of issues`
- `88bfb68` `2026-01-16` `fix(settings): default image rename offer to visual availability`
- `78cc5a8` `2026-01-16` `fix(analyze-flow): remove network check if using local LLM`
- `3912cb0` `2026-01-16` `test(support): make prompt threshold checks dynamic`
- `c347184` `2026-01-16` `docs(tests): add Catch2 listing commands`

## Motivation
The Q4/Q8 model-selection work was only part of the January local-LLM cleanup. The rest of the effort was about avoiding unnecessary network checks in local-only flows, defaulting rename offers sanely when visual analysis is unavailable, and making test output easier to inspect while support thresholds were being tuned.

## What changed
This grouped set improved CPU fallback behavior, removed irrelevant network checks from local analysis, defaulted image rename offers based on visual-LLM availability, made threshold tests dynamic, and documented Catch2 test-listing usage for contributors.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `d7d4ca1`
```diff
diff --git a/app/include/LocalLLMClient.hpp b/app/include/LocalLLMClient.hpp
--- a/app/include/LocalLLMClient.hpp
+++ b/app/include/LocalLLMClient.hpp
@@ -30,8 +30,8 @@ private:
     void load_model_if_needed();
     void configure_llama_logging(const std::shared_ptr<spdlog::logger>& logger) const;
     llama_model_params prepare_model_params(const std::shared_ptr<spdlog::logger>& logger);
-    void load_model_or_throw(const llama_model_params& model_params,
-                             const std::shared_ptr<spdlog::logger>& logger);
+    llama_model_params load_model_or_throw(llama_model_params model_params,
+                                           const std::shared_ptr<spdlog::logger>& logger);
     void configure_context(int context_length, const llama_model_params& model_params);
 
     std::string model_path;
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index ee09828..1aa7c49 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ee09828cb057460b369576410601a3a09279e23c
+Subproject commit 1aa7c497c5e1929771c4fc3fc8e37c7157fa9bbd
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `88bfb68`
```diff
diff --git a/app/lib/Settings.cpp b/app/lib/Settings.cpp
--- a/app/lib/Settings.cpp
+++ b/app/lib/Settings.cpp
@@ -201,6 +201,7 @@ Settings::Settings()
     language = system_default_language();
     category_language = CategoryLanguage::English;
     analyze_images_by_content = visual_llm_files_available();
+    offer_rename_images = analyze_images_by_content;
 }
 
 LLMChoice Settings::parse_llm_choice() const
```

This second excerpt is included because `88bfb68` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
