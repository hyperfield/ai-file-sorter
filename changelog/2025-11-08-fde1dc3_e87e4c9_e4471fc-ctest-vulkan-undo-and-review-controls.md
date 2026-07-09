# 2025-11-08: CTest integration, Vulkan support, undo, and richer review controls

## Covered commits
- `fde1dc3` `2025-11-07` `feat(app): add support for Vulkan`
- `e87e4c9` `2025-11-08` `feat(tests): add ctest and tests`
- `e4471fc` `2025-11-08` `feat(ui): add the file sort undo action`
- `83324e5` `2025-11-08` `feat(ui): control subcategories in the categorization review dialog`
- `74e0180` `2025-11-08` `feat(tests): add rebuilt & test script for linux`
- `c0e547a` `2025-11-08` `feat(tests): add test hooks`
- `d4588f3` `2025-11-08` `feat(tests): add core tests`
- `49d036c` `2025-11-08` `feat(tests): new`
- `2f630e9` `2025-11-08` `chore(starapp): add startup flags`
- `0280a16` `2025-11-08` `fix(file-explorer): double-click on folder to expand (left)`

## Motivation
This was the first big usability-and-reliability milestone after the Qt6 transition. The project needed formal automated tests, broader GPU coverage beyond CUDA, and safer post-analysis controls so users could review and undo more confidently.

## What changed
The grouped work added Vulkan runtime support, introduced the CTest-driven test suite and hooks, added the undo-last-sort path, improved the categorization review controls, and tightened explorer interactions and startup flag handling around the same release window.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `fde1dc3`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -121,6 +121,7 @@ AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* an
 - **Compiler**: A C++20-capable compiler (`g++` or `clang++`).
 - **Qt 6**: Core, Gui, Widgets modules and the Qt resource compiler (`qt6-base-dev` / `qt6-tools` on Linux, `brew install qt` on macOS).
 - **Libraries**: `curl`, `sqlite3`, `fmt`, `spdlog`, and the prebuilt `llama` libraries shipped under `app/lib/precompiled`.
+- **Optional GPU backends**: CUDA 12.x for NVIDIA cards, or a Vulkan 1.2+ driver for AMD/NVIDIA/Intel GPUs. If neither is present, the app falls back to CPU/OpenBLAS automatically.
 - **Git** (optional): For cloning this repository. Archives can also be downloaded.
 - **OpenAI API Key** (optional): Required only when using the remote ChatGPT workflow.
 
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
index d13d0d0..2e93b49 100644
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -257,17 +257,22 @@ if(WIN32)
 
     set(PRECOMPILED_CPU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/cpu")
     set(PRECOMPILED_CUDA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/cuda")
+    set(PRECOMPILED_VULKAN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/vulkan")
 
     add_custom_command(TARGET aifilesorter POST_BUILD
         # COMMAND ${CMAKE_COMMAND} -E remove -f "$<TARGET_FILE_DIR:aifilesorter>/openblas.dll"
         COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wocuda"
         COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wcuda"
+        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wvulkan"
         COMMAND ${CMAKE_COMMAND} -E copy_directory
             "${PRECOMPILED_CPU_ROOT}"
             "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wocuda"
         COMMAND ${CMAKE_COMMAND} -E copy_directory
             "${PRECOMPILED_CUDA_ROOT}"
             "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wcuda"
+        COMMAND ${CMAKE_COMMAND} -E copy_directory
+            "${PRECOMPILED_VULKAN_ROOT}"
+            "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wvulkan"
     )
 
     # set(_precompiled_libopenblas "${PRECOMPILED_CPU_ROOT}/bin/libopenblas.dll")
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `e87e4c9`
```diff
diff --git a/.ccache/9/9/stats b/.ccache/9/9/stats
--- /dev/null
+++ b/.ccache/9/9/stats
@@ -0,0 +1,42 @@
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+0
+1
+0
+0
+1
+0
+0
+0
+0
+0
diff --git a/.gitmodules b/.gitmodules
index b32414d..bd5c35d 100644
--- a/.gitmodules
+++ b/.gitmodules
@@ -1,3 +1,6 @@
 [submodule "app/include/external/llama.cpp"]
 	path = app/include/external/llama.cpp
 	url = https://github.com/ggerganov/llama.cpp.git
+[submodule "external/Catch2"]
+	path = external/Catch2
+	url = https://github.com/catchorg/Catch2.git
```

This second excerpt is included because `e87e4c9` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
