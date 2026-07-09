# 2026-01-06: docs(changelog): add 2026-01-06 visual LLM summary

## Covered commits
- `413c5c3` `2026-01-06` `docs(changelog): add 2026-01-06 visual LLM summary`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `changelog/2026-01-06-changelog-visual-llm-rename-cache-build.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `changelog/2026-01-06-changelog-visual-llm-rename-cache-build.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `413c5c3`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/changelog/2026-01-06-changelog-visual-llm-rename-cache-build.md b/changelog/2026-01-06-changelog-visual-llm-rename-cache-build.md
--- /dev/null
+++ b/changelog/2026-01-06-changelog-visual-llm-rename-cache-build.md
@@ -0,0 +1,414 @@
+# 2026-01-06 - Visual LLM, rename cache, GPU/backends, build/tests
+
+This changelog captures the changes I implemented during this chat session. The goals were to:
+
+- keep partial work when Cancel is pressed (finish the current item, then stop cleanly),
+- show visual LLM batch progress during image decoding,
+- support image rename-only flows (and reuse the image analysis output for categorization),
+- persist suggested filenames in the cache database,
+- fix rename-only behavior after Undo and surface accurate statuses in the Review dialog,
+- add explicit GPU backend selection and safer auto-estimates,
+- stabilize builds for mtmd progress callbacks and mtmd-cli, and
+- document runtime knobs and add tests to cover the new flows.
+
+## Visual LLM analysis pipeline (image handling, progress, cancel)
+
+Motivation: image analysis needs visible progress, should allow Cancel to stop after the current file, and should reuse image analysis output rather than recomputing prompts. It also needs a clean rename-only path.
+
+### New analyzer for image description + filename
+
+Created `app/include/LlavaImageAnalyzer.hpp` and `app/lib/LlavaImageAnalyzer.cpp` to encapsulate LLaVA usage and filename suggestions. It generates a description, then a short filename prompt, and normalizes the name to the original extension:
+
+```cpp
+LlavaImageAnalysisResult LlavaImageAnalyzer::analyze(const std::filesystem::path& image_path) {
+    const std::string description = infer_text(bitmap.get(),
+                                               build_description_prompt(),
+                                               settings_.n_predict);
+
+    const std::string raw_filename = infer_text(nullptr,
+                                                build_filename_prompt(description),
+                                                settings_.n_predict);
+    std::string filename_base = sanitize_filename(raw_filename, kMaxFilenameWords, kMaxFilenameLength);
+    if (filename_base.empty()) {
+        filename_base = sanitize_filename(description, kMaxFilenameWords, kMaxFilenameLength);
+    }
+
+    LlavaImageAnalysisResult result;
+    result.description = description;
+    result.suggested_name = normalize_filename(filename_base, image_path);
+    return result;
+}
+```
+
+### Batch progress callback for image decoding
+
+The visual decoder is broken into batches, so I hooked mtmd progress callbacks to report `current_batch/total_batches` back to the UI:
+
+```cpp
+void LlavaImageAnalyzer::mtmd_progress_callback(const char* name,
+                                                int32_t current_batch,
+                                                int32_t total_batches,
+                                                void* user_data) {
+    if (name && std::strcmp(name, "image") != 0) {
+        return;
+    }
+    auto* self = static_cast<LlavaImageAnalyzer*>(user_data);
+    if (self->settings_.batch_progress) {
+        self->settings_.batch_progress(current_batch, total_batches);
+    }
+}
+```
+
+The callback is registered only when the progress API is available (see build fixes below):
+
+```cpp
+#ifdef AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
+struct ProgressGuard {
+    bool active{false};
+    ProgressGuard(bool enabled, LlavaImageAnalyzer* self) : active(enabled) {
+        if (active) {
+            mtmd_helper_set_progress_callback(&LlavaImageAnalyzer::mtmd_progress_callback, self);
+        }
+    }
+    ~ProgressGuard() {
+        if (active) {
+            mtmd_helper_set_progress_callback(nullptr, nullptr);
+        }
+    }
+};
+#endif
+```
+
+### Main analysis flow changes (cancel, batch progress, rename-only, prompt override)
+
+`app/lib/MainApp.cpp` was updated to:
+
+- track a `stop_requested` flag that is latched when Cancel is pressed, allowing the current file to finish before stopping;
+- split image entries vs. non-image entries;
+- run image analysis first, store per-image metadata in `image_info`, and reuse that for categorization;
+- emit progress lines for image decoding batches;
+- support "rename images only" without running text categorization for those images.
+
+```cpp
+bool stop_requested = false;
+auto update_stop = [this, &stop_requested]() {
+    if (!stop_requested && should_abort_analysis()) {
+        stop_requested = true;
```

The excerpt is taken from the commit diff for `docs(changelog): add 2026-01-06 visual LLM summary`. The most relevant surfaces are `changelog/2026-01-06-changelog-visual-llm-rename-cache-build.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
