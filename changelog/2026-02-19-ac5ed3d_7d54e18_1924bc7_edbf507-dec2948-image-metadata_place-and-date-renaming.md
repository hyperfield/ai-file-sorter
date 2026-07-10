# 2026-02-19: Image metadata extraction, place/date rename support, and date-category suffixing

## Covered commits
- `ac5ed3d` `2026-02-19` `feat(image-metadata): add EXIF service for jpeg/tiff/png with heic fallback`
- `7d54e18` `2026-02-19` `feat(ui): add photo date/place rename option and wire settings`
- `1924bc7` `2026-02-19` `feat(image-analysis): add image-date category option and EXIF date category suffixing`
- `edbf507` `2026-02-19` `chore(i18n): add translations for new image metadata UI strings`
- `dec2948` `2026-02-19` `docs(readme,changelog): document image date-to-category option`
- `2c3731d` `2026-02-19` `docs(image-metadata): add doxygen blocks`
- `1bdba69` `2026-02-19` `chore(release): bump version to 1.7.0 and update changelog`

## Motivation
The visual-LLM workflow could describe images, but users also wanted deterministic metadata-based naming improvements where EXIF data already existed. Adding place/date rename options and date-derived category suffixes was justified because those signals are often more precise and cheaper than inference alone.

## What changed
This grouped feature set added EXIF-based image metadata extraction, place/date filename support, image-date category suffixing, UI strings and translations for the new controls, README/changelog documentation, Doxygen blocks for the metadata service, and the 1.7.0 version bump.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `ac5ed3d`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -523,6 +523,7 @@ if(AI_FILE_SORTER_BUILD_TESTS)
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_whitelist_and_prompt.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_database_manager_rename_only.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_cache_interactions.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_image_rename_metadata_service.cpp"
     )
 
     target_include_directories(ai_file_sorter_tests PRIVATE
diff --git a/app/include/ImageRenameMetadataService.hpp b/app/include/ImageRenameMetadataService.hpp
new file mode 100644
index 0000000..5ea94bf
--- /dev/null
+++ b/app/include/ImageRenameMetadataService.hpp
@@ -0,0 +1,83 @@
+#ifndef IMAGE_RENAME_METADATA_SERVICE_HPP
+#define IMAGE_RENAME_METADATA_SERVICE_HPP
+
+#include <chrono>
+#include <filesystem>
+#include <optional>
+#include <string>
+
+struct sqlite3;
+
+/**
+ * @brief Enriches image rename suggestions using EXIF date + reverse-geocoded place.
+ *
+ * Metadata sources:
+ * - JPEG APP1 EXIF
+ * - TIFF native EXIF
+ * - PNG eXIf chunk
+ * - HEIC/HEIF via exiftool fallback (when available in PATH)
+ */
+class ImageRenameMetadataService {
+public:
+    explicit ImageRenameMetadataService(std::string config_dir);
+    ~ImageRenameMetadataService();
+
+    ImageRenameMetadataService(const ImageRenameMetadataService&) = delete;
+    ImageRenameMetadataService& operator=(const ImageRenameMetadataService&) = delete;
+
+    /**
+     * @brief Adds metadata prefixes to a suggested image filename when available.
+     *
+     * Prefix order is date first, then place, e.g. `2014-03-10_venice_black_ducks.jpg`.
+     * If EXIF metadata is missing or place lookup cannot be done, the original
+     * suggestion is returned unchanged.
+     */
+    std::string enrich_suggested_name(const std::filesystem::path& image_path,
+                                      const std::string& suggested_name);
+
+    /**
+     * @brief Utility used by tests and callers to compose prefixed filenames.
+     */
+    static std::string apply_prefix_to_filename(const std::string& suggested_name,
+                                                const std::optional<std::string>& date_prefix,
+                                                const std::optional<std::string>& place_prefix);
+
+private:
+    struct ExifMetadata {
+        std::optional<std::string> capture_date;
+        std::optional<double> latitude;
+        std::optional<double> longitude;
+    };
+
+    struct CacheLookup {
+        bool found{false};
+        std::optional<std::string> place;
+    };
+
+    struct ReverseGeocodeResult {
+        bool success{false};
+        std::optional<std::string> place;
+    };
+
+    bool open_cache_db();
+    ExifMetadata extract_exif_metadata(const std::filesystem::path& image_path) const;
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `7d54e18`
```diff
diff --git a/app/include/MainApp.hpp b/app/include/MainApp.hpp
--- a/app/include/MainApp.hpp
+++ b/app/include/MainApp.hpp
@@ -219,6 +219,7 @@ private:
     QPointer<QCheckBox> include_subdirectories_checkbox;
     QPointer<QCheckBox> analyze_images_checkbox;
     QPointer<QCheckBox> process_images_only_checkbox;
+    QPointer<QCheckBox> add_image_date_place_to_filename_checkbox;
     QPointer<QCheckBox> offer_rename_images_checkbox;
     QPointer<QCheckBox> rename_images_only_checkbox;
     QPointer<QToolButton> image_options_toggle_button;
diff --git a/app/include/MainAppTestAccess.hpp b/app/include/MainAppTestAccess.hpp
index afb22a8..b089c27 100644
--- a/app/include/MainAppTestAccess.hpp
+++ b/app/include/MainAppTestAccess.hpp
@@ -64,6 +64,12 @@ public:
      * @return Pointer to the checkbox, or nullptr if unavailable.
      */
     static QCheckBox* process_images_only_checkbox(MainApp& app);
+    /**
+     * @brief Access the \"Add photo date and place to filename\" checkbox.
+     * @param app MainApp instance.
+     * @return Pointer to the checkbox, or nullptr if unavailable.
+     */
+    static QCheckBox* add_image_date_place_to_filename_checkbox(MainApp& app);
     /**
      * @brief Access the \"Offer to rename picture files\" checkbox.
      * @param app MainApp instance.
```

This second excerpt is included because `7d54e18` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
