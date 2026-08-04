# 2026-01-11: Windows build fixes, Linux launcher arguments, install target repair, and support-threshold tuning

## Covered commits
- `d96ca85` `2026-01-11` `fix(windows-build): adaptations for windows and algo fix and improvement`
- `37d2222` `2026-01-10` `fix(linux-run): cli parameters`
- `ad5b4e0` `2026-01-11` `fix(makefile): target for install`
- `6251358` `2026-01-11` `chore(support-dialog): update thresholds`

## Motivation
The 1.5.0 release period left a handful of practical follow-ups: the Windows build still needed cleanup, the Linux launcher argument handling needed to match the new processing modes, `make install` needed correction, and the support-dialog thresholds had to be recalibrated against the new feature surface.

## What changed
These commits corrected the Windows build and launcher logic, fixed the Linux run-path parameter handling, repaired the install target, and adjusted support-threshold behavior so the post-release experience was less brittle.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `d96ca85`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -10,6 +10,7 @@ set(CMAKE_CXX_EXTENSIONS OFF)
 option(AI_FILE_SORTER_BUILD_TESTS "Build unit tests (requires Catch2 submodule)" OFF)
 
 include(CheckSymbolExists)
+include(CheckCXXSourceCompiles)
 
 # Prefer MSVC on Windows if available
 if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
@@ -146,6 +147,43 @@ if(WIN32)
             IMPORTED_LOCATION "${dll_path}"
         )
     endforeach()
+
+    set(MTMD_IMPORT "${PRECOMPILED_CPU_LIB_DIR}/mtmd.lib")
+    set(MTMD_DLL "${PRECOMPILED_CPU_BIN_DIR}/mtmd.dll")
+    if(EXISTS "${MTMD_IMPORT}" AND EXISTS "${MTMD_DLL}")
+        add_library(mtmd SHARED IMPORTED)
+        set_target_properties(mtmd PROPERTIES
+            IMPORTED_IMPLIB "${MTMD_IMPORT}"
+            IMPORTED_LOCATION "${MTMD_DLL}"
+        )
+        target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_HAS_MTMD)
+        set(_aifs_saved_required_libs "${CMAKE_REQUIRED_LIBRARIES}")
+        set(CMAKE_REQUIRED_LIBRARIES "${MTMD_IMPORT}")
+        check_cxx_source_compiles([[
+            extern "C" void mtmd_helper_set_progress_callback(void*, void*);
+            int main() {
+                mtmd_helper_set_progress_callback(0, 0);
+                return 0;
+            }
+        ]] HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
+        check_cxx_source_compiles([[
+            extern "C" void mtmd_helper_log_set(void*, void*);
+            int main() {
+                mtmd_helper_log_set(0, 0);
+                return 0;
+            }
+        ]] HAVE_MTMD_HELPER_LOG_SET)
+        set(CMAKE_REQUIRED_LIBRARIES "${_aifs_saved_required_libs}")
+        unset(_aifs_saved_required_libs)
+        if(HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
+            target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
+        endif()
+        if(HAVE_MTMD_HELPER_LOG_SET)
+            target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_LOG_CALLBACK)
+        endif()
+    else()
+        message(WARNING "Visual LLM support disabled: missing mtmd.lib/mtmd.dll under app/lib/precompiled/cpu. Run app/scripts/build_llama_windows.ps1 to stage mtmd.")
+    endif()
 else()
     # Build llama.cpp from the included submodule for non-Windows platforms
     set(LLAMA_BUILD_COMMON ON CACHE BOOL "llama: build common utils library" FORCE)
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `37d2222`
```diff
diff --git a/app/scripts/run_aifilesorter.sh.in b/app/scripts/run_aifilesorter.sh.in
--- a/app/scripts/run_aifilesorter.sh.in
+++ b/app/scripts/run_aifilesorter.sh.in
@@ -43,10 +43,10 @@ CUDA_OVERRIDE=""
 VULKAN_OVERRIDE=""
 for arg in "$@"; do
     case "$arg" in
-        --cuda=on) CUDA_OVERRIDE="on" ;;
-        --cuda=off) CUDA_OVERRIDE="off" ;;
-        --vulkan=on) VULKAN_OVERRIDE="on" ;;
-        --vulkan=off) VULKAN_OVERRIDE="off" ;;
+        --cuda=on|cuda=on) CUDA_OVERRIDE="on" ;;
+        --cuda=off|cuda=off) CUDA_OVERRIDE="off" ;;
+        --vulkan=on|vulkan=on) VULKAN_OVERRIDE="on" ;;
+        --vulkan=off|vulkan=off) VULKAN_OVERRIDE="off" ;;
     esac
 done
 
@@ -96,9 +96,9 @@ elif [ "$VULKAN_OVERRIDE" = "off" ]; then
 fi
 
 if [ $USE_CUDA -eq 0 ] && [ $USE_VULKAN -eq 0 ]; then
-    if [ -n "$SELECTED_VK_DIR" ]; then
+    if [ "$VULKAN_OVERRIDE" != "off" ] && [ -n "$SELECTED_VK_DIR" ]; then
         USE_VULKAN=1
-    elif [ -n "$SELECTED_CUDA_DIR" ]; then
+    elif [ "$CUDA_OVERRIDE" != "off" ] && [ -n "$SELECTED_CUDA_DIR" ]; then
         USE_CUDA=1
     fi
 fi
```

This second excerpt is included because `37d2222` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
