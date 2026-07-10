# 2026-01-07: macOS fixes for the new visual-LLM path

## Covered commits
- `a53d7ac` `2026-01-07` `fix(image-analysis): fixes for macOS`

## Motivation
The first image-analysis release immediately exposed macOS-specific rough edges. A dedicated fix chapter is warranted because platform-specific runtime issues in the new visual stack could block the feature entirely for Apple users even when the broader implementation was correct.

## What changed
This isolated fix adjusted the image-analysis path for macOS so the new LLaVA flow behaved correctly on that platform rather than only on Linux and Windows.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `a53d7ac`
```diff
diff --git a/app/include/LlavaImageAnalyzer.hpp b/app/include/LlavaImageAnalyzer.hpp
--- a/app/include/LlavaImageAnalyzer.hpp
+++ b/app/include/LlavaImageAnalyzer.hpp
@@ -7,6 +7,7 @@
 #include <string>
 
 #ifdef AI_FILE_SORTER_HAS_MTMD
+#include "ggml.h"
 struct llama_model;
 struct llama_context;
 struct llama_vocab;
@@ -81,6 +82,11 @@ private:
                                        int32_t current_batch,
                                        int32_t total_batches,
                                        void* user_data);
+#ifndef AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
+    static void mtmd_log_callback(enum ggml_log_level level,
+                                  const char* text,
+                                  void* user_data);
+#endif
 #endif
     Settings settings_;
 };
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
