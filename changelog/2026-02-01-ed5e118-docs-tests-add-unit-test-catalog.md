# 2026-02-01: docs(tests): add unit test catalog

## Covered commits
- `ed5e118` `2026-02-01` `docs(tests): add unit test catalog`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `TESTS.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `TESTS.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `ed5e118`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/TESTS.md b/TESTS.md
--- /dev/null
+++ b/TESTS.md
@@ -0,0 +1,407 @@
+# Test Suite Guide
+
+This document provides a detailed, textbook-style description of every test case in the project. It is organized by test file and mirrors the intent, setup, procedure, and expected outcomes for each case. All unit tests live under `tests/unit`. Some UI-centric tests are compiled only on non-Windows platforms and use the Qt offscreen platform plugin so they can run without a visible display.
+
+## How to run tests
+- Build and run all tests: `cmake --build build-tests` then `ctest --test-dir build-tests --output-on-failure -j $(nproc)`
+- Run a single test case by name: `./build-tests/ai_file_sorter_tests "<test case name or pattern>"`
+
+## Unit test catalog
+
+### `tests/unit/test_local_llm_backend.cpp` (skipped when `GGML_USE_METAL` is defined)
+
+#### Test case: detect_preferred_backend reads environment
+Purpose: Verify that the backend preference resolver honors the explicit environment override.
+Setup: Set `AI_FILE_SORTER_GPU_BACKEND` to `cuda` via an environment guard.
+Procedure: Call `detect_preferred_backend()` through the test access layer.
+Expected outcome: The detected preference is `Cuda`.
+
+#### Test case: CPU backend is honored when forced
+Purpose: Ensure the GPU layer count is forced to CPU when the backend is set to CPU.
+Setup: Create a temporary GGUF model file and set `AI_FILE_SORTER_GPU_BACKEND=cpu`. Ensure no CUDA disable flag or layer override is set.
+Procedure: Call `prepare_model_params_for_testing()` for the temporary model.
+Expected outcome: `n_gpu_layers` is `0`.
+
+#### Test case: CUDA backend can be forced off via GGML_DISABLE_CUDA
+Purpose: Confirm that the global CUDA disable flag overrides a CUDA backend preference.
+Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda` and `GGML_DISABLE_CUDA=1`. Inject a probe that reports CUDA available.
+Procedure: Call `prepare_model_params_for_testing()`.
+Expected outcome: `n_gpu_layers` is `0`, indicating CPU fallback.
+
+#### Test case: CUDA override is applied when backend is available
+Purpose: Validate that an explicit layer override is used when CUDA is available.
+Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, set `AI_FILE_SORTER_N_GPU_LAYERS=7`, and inject a CUDA-available probe.
+Procedure: Call `prepare_model_params_for_testing()`.
+Expected outcome: `n_gpu_layers` equals `7`.
+
+#### Test case: CUDA fallback when no GPU is available
+Purpose: Ensure CUDA preference falls back when no GPU is detected.
+Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, leave layer override unset, and inject a CUDA-unavailable probe.
+Procedure: Call `prepare_model_params_for_testing()`.
+Expected outcome: `n_gpu_layers` is `0` or `-1` (CPU or auto fallback).
+
+#### Test case: Vulkan backend honors explicit override
+Purpose: Check that Vulkan backend respects a specific GPU layer override.
+Setup: Set `AI_FILE_SORTER_GPU_BACKEND=vulkan`, set `AI_FILE_SORTER_N_GPU_LAYERS=12`, and provide a memory probe that returns no data.
+Procedure: Call `prepare_model_params_for_testing()`.
+Expected outcome: `n_gpu_layers` equals `12`.
+
+#### Test case: Vulkan backend derives layer count from memory probe
+Purpose: Verify that Vulkan backend derives a sensible layer count from reported GPU memory.
+Setup: Use a model with 48 blocks, set `AI_FILE_SORTER_GPU_BACKEND=vulkan`, and inject a probe reporting a 3 GB discrete GPU.
+Procedure: Call `prepare_model_params_for_testing()`.
+Expected outcome: `n_gpu_layers` is greater than `0` and less than or equal to `48`.
+
+### `tests/unit/test_main_app_image_options.cpp` (non-Windows only)
+
+#### Test case: Image analysis checkboxes enable and enforce rename-only behavior
+Purpose: Ensure the image analysis options enable correctly and enforce the rename-only rule.
+Setup: Create dummy LLaVA model files, configure settings with image analysis and rename options off, and construct `MainApp` with offscreen Qt.
+Procedure: Toggle the "Analyze picture files" checkbox on, then toggle the "Do not categorize picture files" checkbox on and attempt to unset "Offer to rename picture files".
+Expected outcome: The option group enables when analysis is checked; enabling rename-only forces offer-rename on; disabling offer-rename clears rename-only.
+
+#### Test case: Image rename-only does not disable categorization unless processing images only
+Purpose: Confirm that rename-only for images does not disable file categorization by itself.
+Setup: Initialize settings with image analysis off and build `MainApp` with offscreen Qt.
+Procedure: Enable image analysis and rename-only, then check whether "Categorize files" remains enabled. Next, enable "Process picture files only".
+Expected outcome: Categorization remains enabled with rename-only, but becomes disabled when processing images only.
+
+#### Test case: Document rename-only does not disable categorization unless processing documents only
+Purpose: Mirror the image-only behavior for documents.
+Setup: Initialize settings with document analysis off and build `MainApp` with offscreen Qt.
+Procedure: Enable document analysis and rename-only, then check whether "Categorize files" remains enabled. Next, enable "Process document files only".
+Expected outcome: Categorization remains enabled with rename-only, but becomes disabled when processing documents only.
+
+#### Test case: Document analysis ignores other files when categorize files is off
+Purpose: Verify the entry splitter respects the "categorize files" flag when only document analysis is active.
+Setup: Prepare a mixed list of image, document, other file, and a directory entry. Set all flags to analyze documents only and categorize files off.
+Procedure: Call `split_entries_for_analysis()` and inspect the output buckets.
+Expected outcome: Document entries are analyzed, other non-document files are excluded, and directories are still included in the "other" bucket.
+
+#### Test case: Image analysis toggle disables when dialog closes without downloads
+Purpose: Ensure the analysis checkbox reverts if the required visual models are not available.
+Setup: Configure settings with image analysis off and inject probes that simulate missing visual models and a prompt acceptance.
+Procedure: Toggle the image analysis checkbox on.
+Expected outcome: The checkbox reverts to unchecked and settings remain unchanged.
+
+#### Test case: Image analysis toggle cancels when user declines download
+Purpose: Verify that declining the download prompt cancels enabling image analysis.
+Setup: Configure settings with image analysis off and inject probes that simulate missing visual models and prompt rejection.
+Procedure: Toggle the image analysis checkbox on.
+Expected outcome: The checkbox remains unchecked, settings remain unchanged, and no download dialog is launched.
+
+#### Test case: Already-renamed images skip vision analysis
+Purpose: Confirm that images already renamed are handled without re-analysis.
+Setup: Provide image entries where one is already renamed and a rename-only flag can be toggled.
+Procedure: Run `split_entries_for_analysis()` in two sections: (a) normal categorization and (b) rename-only enabled.
```

The excerpt is taken from the commit diff for `docs(tests): add unit test catalog`. The most relevant surfaces are `TESTS.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
