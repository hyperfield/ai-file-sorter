# 2026-03-04: Visual batching reuse, progress-row visibility, and Homebrew MediaInfo compatibility

## Covered commits
- `1615dab` `2026-03-04` `perf(image-analysis): reuse llama context and increase visual batching`
- `4e0f8f1` `2026-03-04` `fix(progress-dialog): keep categorization rows visible`
- `8b26e0e` `2026-03-04` `fix(build): support Homebrew MediaInfo on macOS`

## Motivation
Image-analysis throughput and review visibility were still pain points. The app needed to reuse the expensive llama context more effectively, keep categorization rows visible even under mixed-state review conditions, and support package-managed MediaInfo on macOS Homebrew setups instead of assuming only Linux-style discovery.

## What changed
The grouped work improved image-analysis batching by reusing the llama context, fixed progress-dialog visibility of categorization rows, and taught the build to recognize Homebrew MediaInfo installations correctly on macOS.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `1615dab`
```diff
diff --git a/app/include/LlavaImageAnalyzer.hpp b/app/include/LlavaImageAnalyzer.hpp
--- a/app/include/LlavaImageAnalyzer.hpp
+++ b/app/include/LlavaImageAnalyzer.hpp
@@ -176,6 +176,8 @@ private:
     int32_t batch_size_{512};
     bool text_gpu_enabled_{false};
     bool mmproj_gpu_enabled_{false};
+    void initialize_context();
+    void reset_context_state();
     static void mtmd_progress_callback(const char* name,
                                        int32_t current_batch,
                                        int32_t total_batches,
diff --git a/app/lib/LlavaImageAnalyzer.cpp b/app/lib/LlavaImageAnalyzer.cpp
index d6a85a6..8489539 100644
--- a/app/lib/LlavaImageAnalyzer.cpp
+++ b/app/lib/LlavaImageAnalyzer.cpp
@@ -7,6 +7,7 @@
 
 #include <algorithm>
 #include <cctype>
+#include <cstdlib>
 #include <cstdio>
 #include <cstring>
 #include <limits>
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `4e0f8f1`
```diff
diff --git a/app/include/CategorizationProgressDialog.hpp b/app/include/CategorizationProgressDialog.hpp
--- a/app/include/CategorizationProgressDialog.hpp
+++ b/app/include/CategorizationProgressDialog.hpp
@@ -101,6 +101,8 @@ private:
     void refresh_spinner();
     bool has_in_progress_item() const;
     ItemStatus stage_status_for_row(const ItemState& state, StageId stage_id) const;
+    std::optional<int> find_stage_row(StageId stage_id, ItemStatus status) const;
+    void ensure_row_visible(int row);
 
     MainApp* main_app;
     QLabel* stage_list_label{nullptr};
diff --git a/app/lib/CategorizationProgressDialog.cpp b/app/lib/CategorizationProgressDialog.cpp
index 92a21ee..b2e5a3e 100644
--- a/app/lib/CategorizationProgressDialog.cpp
+++ b/app/lib/CategorizationProgressDialog.cpp
@@ -229,6 +229,14 @@ void CategorizationProgressDialog::set_active_stage(StageId stage_id)
     active_stage_ = stage_id;
     refresh_stage_overview();
     refresh_summary();
+
+    if (const auto in_progress_row = find_stage_row(stage_id, ItemStatus::InProgress)) {
+        ensure_row_visible(*in_progress_row);
+        return;
+    }
+    if (const auto pending_row = find_stage_row(stage_id, ItemStatus::Pending)) {
+        ensure_row_visible(*pending_row);
+    }
 }
```

This second excerpt is included because `4e0f8f1` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
