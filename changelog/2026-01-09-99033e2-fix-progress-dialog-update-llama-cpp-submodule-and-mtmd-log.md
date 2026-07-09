# 2026-01-09: fix(progress-dialog): update llama.cpp submodule and MTMD log callback integration

## Covered commits
- `99033e2` `2026-01-09` `fix(progress-dialog): update llama.cpp submodule and MTMD log callback integration`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/CMakeLists.txt`
- `M` `app/Makefile`
- `M` `app/include/LlavaImageAnalyzer.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LlavaImageAnalyzer.cpp`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/CMakeLists.txt`, `app/Makefile`, `app/include/LlavaImageAnalyzer.hpp`, `app/include/external/llama.cpp`, `app/lib/LlavaImageAnalyzer.cpp`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `99033e2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -9,6 +9,8 @@ set(CMAKE_CXX_EXTENSIONS OFF)
 
 option(AI_FILE_SORTER_BUILD_TESTS "Build unit tests (requires Catch2 submodule)" OFF)
 
+include(CheckSymbolExists)
+
 # Prefer MSVC on Windows if available
 if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
     add_definitions(-D_CRT_SECURE_NO_WARNINGS)
@@ -153,7 +155,20 @@ else()
     target_link_libraries(aifilesorter PRIVATE llama)
     target_link_libraries(aifilesorter PRIVATE mtmd)
     target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_HAS_MTMD)
-    target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
+    set(CMAKE_REQUIRED_INCLUDES
+        "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/tools/mtmd"
+        "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/include"
+        "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/ggml/include"
+    )
+    check_symbol_exists(mtmd_helper_set_progress_callback "mtmd-helper.h" HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
+    check_symbol_exists(mtmd_helper_log_set "mtmd-helper.h" HAVE_MTMD_HELPER_LOG_SET)
+    set(CMAKE_REQUIRED_INCLUDES)
+    if(HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
+        target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
+    endif()
+    if(HAVE_MTMD_HELPER_LOG_SET)
+        target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_LOG_CALLBACK)
+    endif()
     if(MSVC)
         target_compile_options(llama PRIVATE /Zc:char8_t-)
     endif()
```

The excerpt is taken from the commit diff for `fix(progress-dialog): update llama.cpp submodule and MTMD log callback integration`. The most relevant surfaces are `app/CMakeLists.txt`, `app/Makefile`, `app/include/LlavaImageAnalyzer.hpp`, `app/include/external/llama.cpp`, `app/lib/LlavaImageAnalyzer.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
