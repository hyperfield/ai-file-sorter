# 2026-02-26: Media metadata rename flow, MediaInfo documentation, and Vulkan-to-CPU fallback follow-ups

## Covered commits
- `69a3e8a` `2026-02-26` `feat(media-rename): add audio/video metadata-based filename suggestions and UI toggle`
- `b83019b` `2026-02-26` `fix(local-llm): fallback to CPU when Vulkan is unavailable`
- `228e6f5` `2026-02-26` `fix(local-llm): fallback to CPU when Vulkan is unavailable`
- `0d2b37b` `2026-02-26` `build(mediainfo): add details about MediaInfo`
- `132a3a4` `2026-02-27` `chore(donation-url): update`
- `e660375` `2026-02-27` `fix: use __has_include for jsoncpp check in GeminiClient.cpp`

## Motivation
This was the audio/video metadata milestone. Once the app could already rename images and documents more intelligently, it was justified to extend that idea to tagged media files. The same window also needed better MediaInfo build documentation and another pass on GPU-to-CPU fallback reliability.

## What changed
The grouped work added audio/video metadata-based filename suggestions and the matching UI toggle, documented MediaInfo package expectations, updated the donation URL, and reinforced the CPU fallback path when Vulkan local-LLM initialization is unavailable. A jsoncpp include-guard fix is grouped here because it removed a portability footgun in the same code path family.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `69a3e8a`
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -221,6 +221,7 @@ private:
     QPointer<QCheckBox> process_images_only_checkbox;
     QPointer<QCheckBox> add_image_date_to_category_checkbox;
     QPointer<QCheckBox> add_image_date_place_to_filename_checkbox;
+    QPointer<QCheckBox> add_audio_video_metadata_to_filename_checkbox;
     QPointer<QCheckBox> offer_rename_images_checkbox;
     QPointer<QCheckBox> rename_images_only_checkbox;
     QPointer<QToolButton> image_options_toggle_button;
diff --git a/app/include/MainAppTestAccess.hpp b/app/include/MainAppTestAccess.hpp
index af163ea..cf71b1b 100644
--- a/app/include/MainAppTestAccess.hpp
+++ b/app/include/MainAppTestAccess.hpp
@@ -76,6 +76,12 @@ public:
      * @return Pointer to the checkbox, or nullptr if unavailable.
      */
     static QCheckBox* add_image_date_place_to_filename_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Add audio/video metadata to file name\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
+    static QCheckBox* add_audio_video_metadata_to_filename_checkbox(MainApp& app);
     /**
      * @brief Access the \"Offer to rename picture files\" checkbox.
      * @param app MainApp instance.
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `b83019b`
```diff
diff --git a/app/include/TestHooks.hpp b/app/include/TestHooks.hpp
--- a/app/include/TestHooks.hpp
+++ b/app/include/TestHooks.hpp
@@ -20,6 +20,10 @@ using BackendMemoryProbe = std::function<std::optional<BackendMemoryInfo>(std::s
 void set_backend_memory_probe(BackendMemoryProbe probe);
 void reset_backend_memory_probe();
 
+using BackendAvailabilityProbe = std::function<bool(std::string_view backend_name)>;
+void set_backend_availability_probe(BackendAvailabilityProbe probe);
+void reset_backend_availability_probe();
+
 using CudaAvailabilityProbe = std::function<bool()>;
 void set_cuda_availability_probe(CudaAvailabilityProbe probe);
 void reset_cuda_availability_probe();
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
index 4070e18..2ba7abb 100644
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -6,7 +6,6 @@
 #include "llama.h"
 #include "gguf.h"
 #include "ggml-backend.h"
-#include "ggml-backend.h"
 #include <string>
 #include <vector>
 #include <cctype>
```

This second excerpt is included because `b83019b` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
