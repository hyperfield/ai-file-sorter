# 2026-03-08: 1.7.0 runtime hardening, Windows build reliability, visual-model partial-download handling, and clearer progress output

## Covered commits
- `6686f16` `2026-03-08` `fix(build): resolve Windows MediaInfo and test runtime setup`
- `a6df55a` `2026-03-08` `fix(image-analysis): harden Windows visual fallback`
- `76290f6` `2026-03-08` `fix(downloads): keep partial visual models out of the live path`
- `f904aa3` `2026-03-08` `fix(ui): use standard disclosure arrows for analysis options`
- `e55e57e` `2026-03-08` `fix(categorization): show current and categorization paths in progress`
- `170c560` `2026-03-08` `fix(i18n): clarify donation dialog wording`
- `3742ae9` `2026-03-08` `fix(i18n): align donation dialog source strings`
- `cf3ec2d` `2026-03-08` `docs(readme): clarify Windows source build setup`
- `5a53f8b` `2026-03-08` `docs(readme): clarify macOS Homebrew pkg-config setup`

## Motivation
The 1.7.0 release was less about introducing a single new feature and more about stabilizing everything added since January. Windows MediaInfo/test runtime setup, visual fallback on Windows, partial visual-model downloads, disclosure-arrow consistency, and progress-path clarity all needed hardening before the release could be trusted broadly.

## What changed
This grouped release chapter covers the Windows build/runtime fixes, hardened visual-analysis fallback on Windows, keeping partial visual-model downloads out of the live path, standardizing disclosure arrows, showing clearer categorization/progress paths, and aligning support-dialog source strings and platform-specific README guidance.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `6686f16`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -42,6 +42,9 @@ endif()
 find_package(fmt REQUIRED CONFIG)
 find_package(spdlog REQUIRED CONFIG)
 find_package(Intl REQUIRED) # libintl/gettext
+# MediaInfoLib from vcpkg depends on the separate ZenLib package and refers to
+# its imported target as plain "zen".
+find_package(ZenLib CONFIG QUIET)
 
 # MediaInfoLib policy:
 # - Must come from package managers (apt/dnf/pacman/brew/vcpkg)
@@ -83,6 +86,12 @@ function(aifs_assert_path_not_in_repo path_value label)
         endif()
 
         file(TO_CMAKE_PATH "${_entry}" _entry_norm)
+        # vcpkg manifest mode can stage package-managed headers/libs inside the
+        # build tree under a local vcpkg_installed directory. That is still a
+        # package-manager origin, not vendored MediaInfo content.
+        if(_entry_norm MATCHES "(^|/)vcpkg_installed(/|$)")
+            continue()
+        endif()
         if(_entry_norm MATCHES "^${_repo_root_regex}(/|$)")
             message(FATAL_ERROR
                 "MediaInfo must come from a package manager. "
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `a6df55a`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -669,6 +669,7 @@ if(AI_FILE_SORTER_BUILD_TESTS)
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_llm_selection_dialog_visual.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_translation.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_image_options.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_visual_fallback.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_settings_image_options.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_ui_translator.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_categorization_dialog.cpp"
@@ -680,6 +681,7 @@ if(AI_FILE_SORTER_BUILD_TESTS)
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_database_manager_rename_only.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_cache_interactions.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_image_rename_metadata_service.cpp"
+        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_llava_image_analyzer.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_media_rename_metadata_service.cpp"
     )
```

This second excerpt is included because `a6df55a` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
