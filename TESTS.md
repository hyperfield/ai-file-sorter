# Test Suite Guide

This document provides a detailed description of every test case in the project. It is organized by test file and mirrors the intent, setup, procedure, and expected outcomes for each case. All unit tests live under `tests/unit`. Some UI-centric tests are compiled only on non-Windows platforms and use the Qt offscreen platform plugin so they can run without a visible display.

## How to run tests
- Configure tests (once): `cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON`
- Build and run all tests: `cmake --build build-tests` then `ctest --test-dir build-tests --output-on-failure -j $(nproc)`
- Run a single test case by name: `./build-tests/ai_file_sorter_tests "<test case name or pattern>"`
- Run GUI test mode: `./build-tests/aifilesorter --test`
- Run production-binary self-tests: `./build-tests/aifilesorter --self-test` or `./build-tests/aifilesorter --self-test=whitelist`
- MediaInfo is expected from a package manager (`apt`/`dnf`/`pacman`/`brew`/`vcpkg`); vendored MediaInfo directories/binaries are intentionally rejected by the build.

## App test modes

The production executable supports two developer-oriented test modes:

- `--test` launches the normal GUI, implies development mode, and adds a Tests menu. The current GUI preset creates a larger sample whitelist and sample files, then runs the normal analysis flow with the selected real LLM for manual review in the Review dialog. It reuses the user's selected LLM settings, but test-mode whitelists, categorization cache, learned behavior, undo data, and sample files are stored under an isolated `test_mode_profile` directory inside the normal config directory.
- `--self-test` runs deterministic headless checks and exits with a pass/fail status. The current suite is `whitelist`, which builds large synthetic whitelists in a temporary config directory and verifies compact prompt candidate selection, learned-category preference, and Unicode whitelist labels. `--self-test` runs all available suites; `--self-test=whitelist` and `--self-test=whitelists` select only the whitelist suite.

## Unit test catalog

### `tests/unit/test_app_test_runner.cpp`

#### Test case: AppTestRunner runs whitelist self-test suite
Purpose: Ensure the production self-test runner can execute the whitelist suite successfully.
Setup: Construct `AppTestRunner` with the `whitelist` suite selector.
Procedure: Run the suite and inspect aggregate and case-level results.
Expected outcome: Three whitelist self-test cases run and all pass.
Run: `./build-tests/ai_file_sorter_tests "AppTestRunner runs whitelist self-test suite"`

#### Test case: AppTestRunner rejects unknown self-test suite
Purpose: Verify unsupported self-test suite names fail clearly.
Setup: Construct `AppTestRunner` with an unknown suite selector.
Procedure: Run the suite and inspect the aggregate result.
Expected outcome: The runner returns an error, no cases are executed, and the aggregate result fails.
Run: `./build-tests/ai_file_sorter_tests "AppTestRunner rejects unknown self-test suite"`

### `tests/unit/test_local_llm_backend.cpp` (skipped when `GGML_USE_METAL` is defined)

#### Test case: detect_preferred_backend reads environment
Purpose: Verify that the backend preference resolver honors the explicit environment override.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND` to `cuda` via an environment guard.
Procedure: Call `detect_preferred_backend()` through the test access layer.
Expected outcome: The detected preference is `Cuda`.
Run: `./build-tests/ai_file_sorter_tests "detect_preferred_backend reads environment"`

#### Test case: Document categorization uses a document-specific system prompt
Purpose: Ensure both analyzable and legacy Office document files use the document-oriented categorization instructions instead of the generic file prompt, including the new allowed-main-category restriction.
Setup: Provide representative `.pdf` and `.doc` paths to the local LLM test access layer.
Procedure: Build the categorization system prompt for each file.
Expected outcome: Both prompts reference filesystem-oriented document categorization guidance and omit the generic installer guidance.
Run: `./build-tests/ai_file_sorter_tests "Document categorization uses a document-specific system prompt"`

#### Test case: Document categorization uses a document-specific user prompt
Purpose: Ensure both summarized document files and legacy Office document files use the dedicated user prompt shape.
Setup: Provide a `.pdf` file name with a `Document summary:` payload, a legacy `.doc` file name without a summary, and a sample consistency context block.
Procedure: Build the categorization user prompt through the local LLM test access layer for both files.
Expected outcome: Both prompts start with the document-specific instruction text, include file name and path, keep the shared guidance, and omit the generic `Full path:` framing; only the summarized file includes a `Document summary:` line.
Run: `./build-tests/ai_file_sorter_tests "Document categorization uses a document-specific user prompt"`

#### Test case: Generic file categorization user prompt includes explicit answer format
Purpose: Keep the generic file prompt short while still forcing the model back into the strict `<Main category> : <Subcategory>` response shape.
Setup: Provide a representative software-like file name and a sample allowed-main-category block.
Procedure: Build the generic categorization user prompt through the local LLM test access layer.
Expected outcome: The prompt includes the full path, file name, allowed main categories, and an explicit one-line answer template.
Run: `./build-tests/ai_file_sorter_tests "Generic file categorization user prompt includes explicit answer format"`

#### Test case: CPU backend is honored when forced
Purpose: Ensure the GPU layer count is forced to CPU when the backend is set to CPU.
Setup: Create a temporary GGUF model file and set `AI_FILE_SORTER_GPU_BACKEND=cpu`. Ensure no CUDA disable flag or layer override is set.
Procedure: Call `prepare_model_params_for_testing()` for the temporary model.
Expected outcome: `n_gpu_layers` is `0`.
Run: `./build-tests/ai_file_sorter_tests "CPU backend is honored when forced"`

#### Test case: CUDA backend can be forced off via GGML_DISABLE_CUDA
Purpose: Confirm that the global CUDA disable flag overrides a CUDA backend preference.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda` and `GGML_DISABLE_CUDA=1`. Inject a backend-availability probe that reports CUDA available.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` is `0`, indicating CPU fallback.
Run: `./build-tests/ai_file_sorter_tests "CUDA backend can be forced off via GGML_DISABLE_CUDA"`

#### Test case: CUDA override is applied when backend is available
Purpose: Validate that an explicit layer override is used when the ggml CUDA backend is available, even if backend memory metrics are absent.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, set `AI_FILE_SORTER_N_GPU_LAYERS=7`, inject a backend-availability probe for CUDA, and inject a backend-memory probe that returns no metrics.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` equals `7`.
Run: `./build-tests/ai_file_sorter_tests "CUDA override is applied when backend is available"`

#### Test case: LocalLLMClient builds a descending GPU-layer retry ladder from optimistic and conservative estimates
Purpose: Ensure GPU model-load retries probe a deterministic sequence before giving up on the backend.
Setup: Provide an optimistic layer count of `20` and a conservative layer count of `15`.
Procedure: Build the retry ladder through the LocalLLM test access helper.
Expected outcome: The ladder is exactly `20, 15, 11, 8, 6, 4, 3, 2, 1`, with each retry smaller than the last.
Run: `./build-tests/ai_file_sorter_tests "LocalLLMClient builds a descending GPU-layer retry ladder from optimistic and conservative estimates"`

#### Test case: LocalLLMClient deduplicates matching retry estimates before reducing GPU layers
Purpose: Avoid redundant GPU reload attempts when optimistic and conservative estimates are the same.
Setup: Provide matching optimistic and conservative layer counts of `15`.
Procedure: Build the retry ladder through the LocalLLM test access helper.
Expected outcome: The ladder starts at `15` once and then descends to `11, 8, 6, 4, 3, 2, 1` without duplicate entries.
Run: `./build-tests/ai_file_sorter_tests "LocalLLMClient deduplicates matching retry estimates before reducing GPU layers"`

#### Test case: CUDA backend reports low GPU memory before load
Purpose: Ensure CUDA preflight checks fall back to CPU before model load when the ggml CUDA backend reports too little available VRAM.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, leave the layer override unset, inject a backend-availability probe for CUDA, and inject backend memory with extremely low free memory.
Procedure: Call `prepare_model_params_result_for_testing()` for a temporary model with enough layers to exceed the reported budget.
Expected outcome: `n_gpu_layers` is `0` and the captured status is `GpuLowMemoryFallbackToCpu`.
Run: `./build-tests/ai_file_sorter_tests "CUDA backend reports low GPU memory before load"`

#### Test case: CUDA backend falls back to CPU when backend memory metrics are unavailable
Purpose: Ensure CUDA preflight declines GPU offload instead of hanging or guessing when the ggml CUDA backend cannot report memory metrics and no explicit layer override is set.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, leave the layer override unset, inject a backend-availability probe for CUDA, and inject a backend-memory probe that returns no data.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` is `0`, indicating a safe CPU fallback.
Run: `./build-tests/ai_file_sorter_tests "CUDA backend falls back to CPU when backend memory metrics are unavailable"`

#### Test case: Auto backend prefers CUDA when both backends are possible
Purpose: Verify that automatic backend selection uses CUDA before Vulkan.
Setup: Leave `AI_FILE_SORTER_GPU_BACKEND` unset, clear `GGML_DISABLE_CUDA`, set `AI_FILE_SORTER_N_GPU_LAYERS=7`, inject a backend-availability probe that reports CUDA available, and inject a backend-memory probe that returns no metrics.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` equals `7`, proving the auto path chose CUDA without consulting Vulkan first.
Run: `./build-tests/ai_file_sorter_tests "Auto backend prefers CUDA when both backends are possible"`

#### Test case: Auto backend falls back to Vulkan when CUDA is disabled
Purpose: Ensure automatic backend selection still reaches Vulkan when CUDA is globally disabled.
Setup: Leave `AI_FILE_SORTER_GPU_BACKEND` unset, set `GGML_DISABLE_CUDA=1`, set `AI_FILE_SORTER_N_GPU_LAYERS=12`, and inject a Vulkan-available probe.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` equals `12`, proving the auto path fell through to Vulkan instead of CPU.
Run: `./build-tests/ai_file_sorter_tests "Auto backend falls back to Vulkan when CUDA is disabled"`

#### Test case: CUDA fallback when no GPU is available
Purpose: Ensure CUDA preference falls back when no ggml CUDA backend is detected.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, leave layer override unset, and inject a backend-availability probe that reports CUDA unavailable.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` is `0`.
Run: `./build-tests/ai_file_sorter_tests "CUDA fallback when no GPU is available"`

#### Test case: Vulkan backend honors explicit override
Purpose: Check that Vulkan backend respects a specific GPU layer override.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=vulkan`, set `AI_FILE_SORTER_N_GPU_LAYERS=12`, and provide a memory probe that returns no data.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` equals `12`.
Run: `./build-tests/ai_file_sorter_tests "Vulkan backend honors explicit override"`

#### Test case: Vulkan backend derives layer count from memory probe
Purpose: Verify that Vulkan backend derives a sensible layer count from reported GPU memory.
Setup: Use a model with 48 blocks, set `AI_FILE_SORTER_GPU_BACKEND=vulkan`, and inject a probe reporting a 3 GB discrete GPU.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` is greater than `0` and less than or equal to `48`.
Run: `./build-tests/ai_file_sorter_tests "Vulkan backend derives layer count from memory probe"`

#### Test case: Vulkan backend reports low GPU memory before load
Purpose: Ensure Vulkan preflight checks fall back to CPU before model load when available VRAM is too low.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=vulkan`, leave the layer override unset, inject a Vulkan-available probe, and inject backend memory with extremely low free memory.
Procedure: Call `prepare_model_params_result_for_testing()` for a temporary model with enough layers to exceed the reported budget.
Expected outcome: `n_gpu_layers` is `0` and the captured status is `GpuLowMemoryFallbackToCpu`.
Run: `./build-tests/ai_file_sorter_tests "Vulkan backend reports low GPU memory before load"`

### `tests/unit/test_single_instance_coordinator.cpp`

#### Test case: SingleInstanceCoordinator notifies the primary instance on relaunch
Purpose: Verify that a second launch notifies the already running primary process instead of becoming a second app instance.
Setup: Create a writable temporary runtime directory, point `AI_FILE_SORTER_SINGLE_INSTANCE_RUNTIME_DIR` at it, start one coordinator as the primary instance, and install an activation callback on it.
Procedure: Verify local socket binding is available, then start a second coordinator with the same instance id and wait for the primary callback to fire.
Expected outcome: When local sockets are available, the second coordinator reports that it is not primary and the first coordinator receives exactly the relaunch activation request. If the sandbox cannot bind local sockets, the test records that the activation handoff is skipped.
Run: `./build-tests/ai_file_sorter_tests "SingleInstanceCoordinator notifies the primary instance on relaunch"`

#### Test case: SingleInstanceCoordinator allows different instance ids to coexist
Purpose: Ensure the coordinator only deduplicates launches that share the same logical app id.
Setup: Create a writable temporary runtime directory, point `AI_FILE_SORTER_SINGLE_INSTANCE_RUNTIME_DIR` at it, and create two coordinators with different unique instance ids.
Procedure: Acquire the primary-instance lock for both coordinators.
Expected outcome: Both coordinators become primary because they represent different logical applications.
Run: `./build-tests/ai_file_sorter_tests "SingleInstanceCoordinator allows different instance ids to coexist"`

### `tests/unit/test_main_app_image_options.cpp` (non-Windows only)

#### Test case: Image analysis checkboxes enable and enforce rename-only behavior
Purpose: Ensure the image analysis options enable correctly and enforce the rename-only rule.
Setup: Create dummy LLaVA model files, configure settings with image analysis and rename options off, construct `MainApp` with offscreen Qt, and stub the visual-LLM availability probe.
Procedure: Toggle the "Analyze picture files" checkbox on, then toggle the "Do not categorize picture files" checkbox on and attempt to unset "Offer to rename picture files".
Expected outcome: The option group enables when analysis is checked; enabling rename-only forces offer-rename on; disabling offer-rename clears rename-only.
Run: `./build-tests/ai_file_sorter_tests "Image analysis checkboxes enable and enforce rename-only behavior"`

#### Test case: Top-level analysis rows share the same leading edge
Purpose: Verify that top-level analysis controls align consistently in the main window.
Setup: Build `MainApp` with offscreen Qt and show the window so widget geometry is calculated.
Procedure: Compare the x-coordinate of the document analysis, image analysis, and audio/video metadata controls.
Expected outcome: All top-level analysis rows share the same leading edge.
Run: `./build-tests/ai_file_sorter_tests "Top-level analysis rows share the same leading edge"`

#### Test case: Analysis toggles use disclosure indicators instead of toolbutton arrows
Purpose: Ensure the image and document option groups use the app's custom disclosure styling.
Setup: Build `MainApp` with offscreen Qt.
Procedure: Inspect the image and document option toggle buttons through the test access layer.
Expected outcome: Both toggles are checkable and use `Qt::NoArrow`, leaving the visible indicator to the styled label.
Run: `./build-tests/ai_file_sorter_tests "Analysis toggles use disclosure indicators instead of toolbutton arrows"`

#### Test case: Main window controls expose accessibility metadata
Purpose: Verify the primary controls expose accessible names and label associations needed by screen readers.
Setup: Build `MainApp` with offscreen Qt and show the window so translated labels and buddies are applied.
Procedure: Inspect the folder label, path entry, main action buttons, and disclosure toggles through the widget tree and test access layer.
Expected outcome: The folder label is linked to the path entry as its buddy, the path entry inherits that accessible name, and the primary buttons and disclosure toggles expose non-empty accessible names.
Run: `./build-tests/ai_file_sorter_tests "Main window controls expose accessibility metadata"`

#### Test case: Progress dialog takes focus and exposes accessibility metadata
Purpose: Ensure analysis startup moves focus into the progress dialog and exposes accessible metadata for its key controls.
Setup: Build `MainApp` with offscreen Qt, construct a `CategorizationProgressDialog`, and configure it with a simple image-analysis stage.
Procedure: Show the progress dialog, process Qt events, and inspect the stop button, dialog metadata, and activity log widget.
Expected outcome: The dialog becomes the active window, the stop button receives focus, and the dialog and log view expose non-empty accessible names or descriptions.
Run: `./build-tests/ai_file_sorter_tests "Progress dialog takes focus and exposes accessibility metadata"`

#### Test case: Image rename-only does not disable categorization unless processing images only
Purpose: Confirm that rename-only for images does not disable file categorization by itself.
Setup: Initialize settings with image analysis off, build `MainApp` with offscreen Qt, and stub the visual-LLM availability probe.
Procedure: Enable image analysis and rename-only, then check whether "Categorize files" remains enabled. Next, enable "Process picture files only".
Expected outcome: Categorization remains enabled with rename-only, but becomes disabled when processing images only.
Run: `./build-tests/ai_file_sorter_tests "Image rename-only does not disable categorization unless processing images only"`

#### Test case: Processing images only disables document analysis controls and audio-video metadata
Purpose: Confirm image-only processing temporarily disables controls that would analyze non-image content.
Setup: Enable image analysis, document analysis, and audio/video metadata in settings, then build `MainApp` with offscreen Qt.
Procedure: Enable "Process picture files only", inspect dependent controls, then disable it again.
Expected outcome: Document analysis controls and audio/video metadata are disabled while image-only mode is active, but the underlying saved settings are preserved and controls re-enable afterward.
Run: `./build-tests/ai_file_sorter_tests "Processing images only disables document analysis controls and audio-video metadata"`

#### Test case: Processing images only preserves recursive scanning when scan subfolders is enabled
Purpose: Ensure image-only processing does not accidentally clear recursive scanning.
Setup: Enable image analysis, image-only processing, and include-subdirectories in settings.
Procedure: Build `MainApp` and read the effective scan options through the test access layer.
Expected outcome: The effective options include both `Files` and `Recursive`.
Run: `./build-tests/ai_file_sorter_tests "Processing images only preserves recursive scanning when scan subfolders is enabled"`

#### Test case: Document rename-only does not disable categorization unless processing documents only
Purpose: Mirror the image-only behavior for documents.
Setup: Initialize settings with document analysis off and build `MainApp` with offscreen Qt.
Procedure: Enable document analysis and rename-only, then check whether "Categorize files" remains enabled. Next, enable "Process document files only".
Expected outcome: Categorization remains enabled with rename-only, but becomes disabled when processing documents only.
Run: `./build-tests/ai_file_sorter_tests "Document rename-only does not disable categorization unless processing documents only"`

#### Test case: Document analysis ignores other files when categorize files is off
Purpose: Verify the entry splitter respects the "categorize files" flag when only document analysis is active.
Setup: Prepare a mixed list of image, document, other file, and a directory entry. Set all flags to analyze documents only and categorize files off.
Procedure: Call `split_entries_for_analysis()` and inspect the output buckets.
Expected outcome: Document entries are analyzed, other non-document files are excluded, and directories are still included in the "other" bucket.
Run: `./build-tests/ai_file_sorter_tests "Document analysis ignores other files when categorize files is off"`

#### Test case: Image analysis toggle disables when dialog closes without downloads
Purpose: Ensure the analysis checkbox reverts if the required visual models are not available.
Setup: Configure settings with image analysis off and inject probes that simulate missing visual models and a prompt acceptance.
Procedure: Toggle the image analysis checkbox on.
Expected outcome: The checkbox reverts to unchecked and settings remain unchanged.
Run: `./build-tests/ai_file_sorter_tests "Image analysis toggle disables when dialog closes without downloads"`

#### Test case: Image analysis toggle cancels when user declines download
Purpose: Verify that declining the download prompt cancels enabling image analysis.
Setup: Configure settings with image analysis off and inject probes that simulate missing visual models and prompt rejection.
Procedure: Toggle the image analysis checkbox on.
Expected outcome: The checkbox remains unchecked, settings remain unchanged, and no download dialog is launched.
Run: `./build-tests/ai_file_sorter_tests "Image analysis toggle cancels when user declines download"`

#### Test case: Already-renamed images skip vision analysis
Purpose: Confirm that images already renamed are handled without re-analysis.
Setup: Provide image entries where one is already renamed and a rename-only flag can be toggled.
Procedure: Run `split_entries_for_analysis()` in two sections: (a) normal categorization and (b) rename-only enabled.
Expected outcome: In normal mode, the already-renamed image is routed to filename-based categorization ("other" bucket). In rename-only mode, the already-renamed image is excluded entirely.
Run: `./build-tests/ai_file_sorter_tests "Already-renamed images skip vision analysis"`

### `tests/unit/test_main_app_visual_fallback.cpp`

#### Test case: Visual CPU fallback detection recognizes retryable GPU failures
Purpose: Ensure visual-analysis GPU failure messages that can be retried on CPU are recognized.
Setup: Provide representative llama context, mtmd, Vulkan, and CUDA memory failure messages.
Procedure: Pass each message through the visual CPU fallback classifier.
Expected outcome: Each retryable GPU failure returns true.
Run: `./build-tests/ai_file_sorter_tests "Visual CPU fallback detection recognizes retryable GPU failures"`

#### Test case: Visual CPU fallback detection ignores non-retryable startup failures
Purpose: Avoid offering CPU retry when the visual model files or projector capabilities are invalid.
Setup: Provide missing model, missing projector, and invalid projector messages.
Procedure: Pass each message through the visual CPU fallback classifier.
Expected outcome: Each non-retryable startup failure returns false.
Run: `./build-tests/ai_file_sorter_tests "Visual CPU fallback detection ignores non-retryable startup failures"`

#### Test case: Visual CPU fallback decline requests analysis cancellation
Purpose: Confirm cancelling the visual CPU retry prompt stops the whole analysis instead of continuing with filename-based categorization.
Setup: Build `MainApp` with an override that declines the visual CPU fallback prompt.
Procedure: Invoke the visual CPU fallback prompt path twice.
Expected outcome: The first decline sets the stop-analysis flag, and the cached decline is reused without prompting again.
Run: `./build-tests/ai_file_sorter_tests "Visual CPU fallback decline requests analysis cancellation"`

#### Test case: Visual CPU fallback acceptance keeps analysis running
Purpose: Confirm accepting visual CPU fallback does not request analysis cancellation.
Setup: Build `MainApp` with an override that accepts the visual CPU fallback prompt.
Procedure: Invoke the visual CPU fallback prompt path.
Expected outcome: The prompt returns true and the stop-analysis flag remains false.
Run: `./build-tests/ai_file_sorter_tests "Visual CPU fallback acceptance keeps analysis running"`

#### Test case: Continue-without-visual-analysis decline requests analysis cancellation
Purpose: Confirm declining filename-only fallback stops the analysis when visual understanding is unavailable.
Setup: Build `MainApp` with an override that declines the continue-without-visual-analysis prompt.
Procedure: Invoke the continue-without-visual-analysis prompt path twice.
Expected outcome: The first decline sets the stop-analysis flag, and the cached decline is reused without prompting again.
Run: `./build-tests/ai_file_sorter_tests "Continue-without-visual-analysis decline requests analysis cancellation"`

#### Test case: Continue-without-visual-analysis acceptance keeps analysis running
Purpose: Confirm accepting filename-only fallback keeps the analysis active when visual understanding is unavailable.
Setup: Build `MainApp` with an override that accepts the continue-without-visual-analysis prompt.
Procedure: Invoke the continue-without-visual-analysis prompt path.
Expected outcome: The prompt returns true and the stop-analysis flag remains false.
Run: `./build-tests/ai_file_sorter_tests "Continue-without-visual-analysis acceptance keeps analysis running"`

#### Test case: Vision diagnostics are only shown in the progress dialog for development or test mode
Purpose: Keep verbose visual runtime and timing diagnostics out of the normal progress dialog while preserving them for developer-oriented runs.
Setup: Build `MainApp` in normal, development, and test modes, then prepare representative `[VISION] Runtime`, `[VISION] Timing`, and ordinary vision progress messages.
Procedure: Query the progress-dialog visibility helper for each mode and message type.
Expected outcome: Normal mode hides the runtime and timing diagnostics but still shows ordinary vision progress, while development and test modes show the diagnostics.
Run: `./build-tests/ai_file_sorter_tests "Vision diagnostics are only shown in the progress dialog for development or test mode"`

### `tests/unit/test_image_analyzer_factory.cpp`

#### Test case: ImageAnalyzerFactory rejects invalid GGUF artifacts before analyzer startup
Purpose: Ensure corrupt or partial visual model artifacts fail before GPU preflight or analyzer construction masks the real issue.
Setup: Build a visual backend descriptor with one-byte `model.gguf` and `mmproj.gguf` files.
Procedure: Call `ImageAnalyzerFactory::create()` with GPU disabled.
Expected outcome: Creation throws a clear invalid/incomplete GGUF artifact error.
Run: `./build-tests/ai_file_sorter_tests "ImageAnalyzerFactory rejects invalid GGUF artifacts before analyzer startup"`

### `tests/unit/test_main_app_cache_action.cpp` (non-Windows only)

#### Test case: Settings maintenance actions stay separate and follow analysis state
Purpose: Ensure learned-behavior reset and cache clearing are separate Settings actions and disabled while analysis is active.
Setup: Build `MainApp` with offscreen Qt using a temporary config directory.
Procedure: Read the Settings menu action order, then toggle the analysis-in-progress state through the test access layer.
Expected outcome: `Reset learned behavior…` appears before `Clear cache…`, both actions start enabled, both become disabled during analysis, and both re-enable afterward.
Run: `./build-tests/ai_file_sorter_tests "Settings maintenance actions stay separate and follow analysis state"`

#### Test case: Plugins menu is only available in development mode
Purpose: Ensure unfinished plugin UI is hidden for public builds while remaining available for developer testing.
Setup: Build one `MainApp` with development mode disabled and one with development mode enabled.
Procedure: Inspect the Plugins menu and Manage Storage Plugins action through the test access layer.
Expected outcome: Public mode exposes neither item; development mode exposes both and the Plugins menu is visible.
Run: `./build-tests/ai_file_sorter_tests "Plugins menu is only available in development mode"`

#### Test case: Tests menu is only available in test mode
Purpose: Ensure real-runtime test presets are hidden unless the app is launched in test mode.
Setup: Build one public `MainApp` and one test-mode `MainApp`.
Procedure: Inspect the Tests menu and large whitelist LLM test action through the test access layer, then simulate analysis in progress.
Expected outcome: Public mode exposes neither item; test mode exposes both, and the action is disabled while analysis is active.
Run: `./build-tests/ai_file_sorter_tests "Tests menu is only available in test mode"`

#### Test case: Test mode can use an isolated runtime data directory
Purpose: Ensure test-mode app-owned data can be redirected away from the normal config directory.
Setup: Build a test-mode `MainApp` with a dedicated test profile path.
Procedure: Let the window initialize whitelists and inspect the generated data files.
Expected outcome: `whitelists.ini` is created in the test profile and not in the normal config directory.
Run: `./build-tests/ai_file_sorter_tests "Test mode can use an isolated runtime data directory"`

### `tests/unit/test_main_app_category_language_menu.cpp` (non-Windows only)

#### Test case: Gemma 3 category language menu exposes the full supported list
Purpose: Ensure the built-in Gemma 3 4B local model exposes the full expanded category-language catalog instead of leaving the long-tail languages hidden.
Setup: Build `MainApp` with offscreen Qt in a temporary config directory and switch settings to `Local_4b_Gemma`.
Procedure: Refresh the category-language menu through the test access layer and collect the visible actions.
Expected outcome: The visible language list matches the full application category-language list exactly.
Run: `./build-tests/ai_file_sorter_tests "Gemma 3 category language menu exposes the full supported list"`

#### Test case: Category language menu follows Mistral 7B support
Purpose: Ensure the Settings -> Category language menu only exposes the conservative, supported translation subset for the built-in Mistral 7B local model.
Setup: Build `MainApp` with offscreen Qt in a temporary config directory and switch settings to `Local_7b`.
Procedure: Refresh the category-language menu through the test access layer and collect the visible actions.
Expected outcome: The visible language list matches the supported Mistral subset exactly.
Run: `./build-tests/ai_file_sorter_tests "Category language menu follows Mistral 7B support"`

#### Test case: English-only local models force the category language menu back to English
Purpose: Ensure built-in local models treated as English-only cannot keep an unsupported non-English category-language selection.
Setup: Build `MainApp` with offscreen Qt in a temporary config directory, preselect French as the category language, and switch to `Local_7b_Gemma`.
Procedure: Refresh the category-language menu through the test access layer.
Expected outcome: Settings fall back to English, only the English menu action remains visible, and that English action is checked.
Run: `./build-tests/ai_file_sorter_tests "English-only local models force the category language menu back to English"`

#### Test case: Custom local models expose the full category language menu
Purpose: Preserve the full Gemma-3-level category-language list for user-supplied local models where the app does not enforce the built-in restrictions.
Setup: Build `MainApp` with offscreen Qt in a temporary config directory and switch settings to `Custom`.
Procedure: Refresh the category-language menu through the test access layer and collect the visible actions.
Expected outcome: The full category-language list remains visible for the custom local model choice.
Run: `./build-tests/ai_file_sorter_tests "Custom local models expose the full category language menu"`

#### Test case: Category language menu keeps visible entries alphabetized
Purpose: Ensure the Settings -> Category language menu stays alphabetized even when the long Gemma/custom language list is rebuilt from grouped submenus and translated labels are active.
Setup: Build `MainApp` with offscreen Qt in a temporary config directory, switch the UI language to French, and set the LLM choice to `Local_4b_Gemma`.
Procedure: Retranslate the window through the test access layer, capture the visible category-language labels, and compare them to a locale-aware sorted copy.
Expected outcome: The translated visible labels are already alphabetized.
Run: `./build-tests/ai_file_sorter_tests "Category language menu keeps visible entries alphabetized"`

#### Test case: Full Gemma 3 category language menus are compartmentalized into submenus
Purpose: Keep the much longer Gemma/custom category-language list usable on smaller screens by grouping it into alphabetic submenus instead of forcing one oversized flat menu.
Setup: Build `MainApp` with offscreen Qt in a temporary config directory and switch settings to `Local_4b_Gemma`.
Procedure: Refresh the category-language menu through the test access layer and inspect the top-level menu actions.
Expected outcome: At least one top-level category-language menu entry is a submenu, confirming the expanded list is compartmentalized.
Run: `./build-tests/ai_file_sorter_tests "Full Gemma 3 category language menus are compartmentalized into submenus"`

### `tests/unit/test_ui_translator.cpp` (non-Windows only)

#### Test case: UiTranslator updates menus, actions, and controls
Purpose: Validate that the UI translator updates all primary controls, menus, and stateful labels in a consistent pass, including category-language actions now generated from the expanded catalog.
Setup: Build a test harness with a `QMainWindow`, many UI controls, a full interface-language action group that now includes Hindi, Simplified Chinese, Swedish, Icelandic, Norwegian, Finnish, Danish, and Korean, plus a generated category-language action array covering the expanded catalog, and a translator state set to French in settings. Use a translation function that returns the input string to test label wiring rather than actual translation files.
Procedure: Call `retranslate_all()` and verify the text of buttons, checkboxes, top-level menus, language menus, representative category-language actions, status labels, and the file explorer dock title. Also verify the interface-language menu is re-sorted alphabetically and the language action group selection stays correct.
Expected outcome: All UI elements show the expected English strings, including File/Edit/View, the interface/category language menus, the `Reset learned behavior…` and `Clear cache…` Settings actions, representative category-language labels such as `Hindi`, `Japanese`, `Simplified Chinese`, and `Traditional Chinese`, the interface-language menu is alphabetized, and the French language action is marked checked, demonstrating the retranslate pipeline is correctly wired.
Run: `./build-tests/ai_file_sorter_tests "*UiTranslator updates menus*"`

### `tests/unit/test_cache_maintenance_service.cpp`

#### Test case: CacheMaintenanceService reports cache paths and sizes
Purpose: Verify the cache-maintenance service resolves the expected categorization, image-location, and log paths and estimates their reclaimable size.
Setup: Create temporary cache files and a temporary log directory, including a rotated log file and a custom categorization-cache filename override.
Procedure: Query each cache target through `target_info()`.
Expected outcome: Each target reports the expected path, existence state, and byte count.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceService reports cache paths and sizes"`

#### Test case: CacheMaintenanceService clears configured cache targets
Purpose: Ensure the default cleanup behavior removes file-backed caches and clears directory-backed log caches.
Setup: Create temporary categorization and image-location cache files plus current, rotated, and nested log files.
Procedure: Call `clear()` for each cache target in turn.
Expected outcome: The cache files are deleted, current log files are truncated, and rotated or nested log entries are removed.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceService clears configured cache targets"`

#### Test case: CacheMaintenanceService uses specialized clear callbacks when provided
Purpose: Confirm app-aware clear callbacks are honored for targets that should not be treated as raw filesystem deletes.
Setup: Construct the service with callback lambdas for the categorization and log targets.
Procedure: Clear those targets and observe the callback flags.
Expected outcome: The callbacks are invoked and the operations report success.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceService uses specialized clear callbacks when provided"`

#### Test case: CacheMaintenanceService reports zero size for an empty categorization database
Purpose: Prevent the categorization cache UI from showing the baseline SQLite schema size after the cache has already been cleared.
Setup: Create a real empty `file_categorization` SQLite database on disk and vacuum it so the file still has a non-zero on-disk size.
Procedure: Query the categorization target through `target_info()`.
Expected outcome: The target still exists on disk, but the reported reclaimable size is `0`, reflecting that no cached categorization rows remain.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceService reports zero size for an empty categorization database"`

### `tests/unit/test_user_learning_store.cpp`

#### Test case: UserLearningStore records approved mappings in a separate database
Purpose: Verify user-approved categorization behavior is persisted outside disposable categorization caches.
Setup: Create a temporary config directory and open `UserLearningStore`.
Procedure: Record an approved mapping, then query the learned taxonomy entry and approved examples.
Expected outcome: The store creates a separate learning database, records the taxonomy entry, and links one approved example to it.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore records approved mappings in a separate database"`

#### Test case: UserLearningStore updates repeated approvals without duplicating examples
Purpose: Ensure repeated approval for the same file key updates the learned mapping instead of growing duplicate examples.
Setup: Create a learning store and record one approved mapping for a file.
Procedure: Record another mapping for the same file key with a different approved category/subcategory.
Expected outcome: The example count remains one, the old taxonomy entry count is refreshed, and the example points at the latest approved taxonomy entry.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore updates repeated approvals without duplicating examples"`

#### Test case: UserLearningStore imports whitelist taxonomy candidates without duplicating entries
Purpose: Seed the learning database from whitelist taxonomy labels without creating approved file examples.
Setup: Create a learning store and prepare repeated whitelist candidates, including a Unicode label.
Procedure: Import the candidates twice and inspect learned taxonomy entries.
Expected outcome: Duplicate candidates are collapsed, Unicode labels are preserved, and approved example count remains zero.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore imports whitelist taxonomy candidates without duplicating entries"`

#### Test case: UserLearningStore stores embeddings for imported taxonomy candidates
Purpose: Verify user taxonomy entries receive persisted embedding vectors immediately after import.
Setup: Import a taxonomy candidate into a fresh learning store.
Procedure: Load the taxonomy entry's embedding record.
Expected outcome: The embedding uses the active local model id, expected dimension, non-empty source hash, and is counted in the learning database.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore stores embeddings for imported taxonomy candidates"`

#### Test case: UserLearningStore preserves review-confirmed taxonomy source during whitelist import
Purpose: Ensure later whitelist imports do not downgrade a category that was explicitly confirmed by the user.
Setup: Import a whitelist candidate, then record an approved mapping for the same category.
Procedure: Import the same whitelist candidate again and inspect the learned taxonomy entry.
Expected outcome: The taxonomy entry keeps source `review_confirmed`, retains its approved example count, and is not duplicated.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore preserves review-confirmed taxonomy source during whitelist import"`

#### Test case: UserLearningStore removes imported whitelist taxonomy candidates without touching approved examples
Purpose: Allow whitelist refreshes to clear imported taxonomy rows without deleting user-approved learning data.
Setup: Import whitelist-sourced candidates, import a non-whitelist user taxonomy candidate, and record a review-confirmed example.
Procedure: Remove candidates whose source starts with `whitelist:` and inspect the remaining taxonomy rows.
Expected outcome: Whitelist-imported entries are removed, user-owned non-whitelist entries remain, and approved examples stay intact.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore removes imported whitelist taxonomy candidates without touching approved examples"`

#### Test case: UserLearningStore refreshes embeddings when approved examples change
Purpose: Ensure stored embeddings track the latest approved example context for a taxonomy entry.
Setup: Record an approved mapping with context text.
Procedure: Capture the stored embedding hash, update the same file approval with different context text, and reload the embedding.
Expected outcome: The embedding source hash changes while the vector shape remains stable.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore refreshes embeddings when approved examples change"`

#### Test case: UserLearningStore retrieves relevant taxonomy candidates from learned examples
Purpose: Verify learned examples can be ranked into candidate categories for future prompts.
Setup: Record an approved manual mapping with filename/context terms and import an unrelated spreadsheet candidate.
Procedure: Retrieve candidates for a camera manual query.
Expected outcome: The manual taxonomy entry is returned first with a positive score and its approved example count.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore retrieves relevant taxonomy candidates from learned examples"`

#### Test case: UserLearningStore uses stored embeddings during candidate retrieval
Purpose: Confirm retrieval can use persisted taxonomy embeddings, not only lexical scoring.
Setup: Record a camera-manual approval and import an unrelated spreadsheet candidate.
Procedure: Retrieve candidates for camera-related terms.
Expected outcome: The manual candidate is ranked first and reports that embedding similarity contributed.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore uses stored embeddings during candidate retrieval"`

#### Test case: UserLearningStore clears learned behavior while keeping the database reusable
Purpose: Verify explicit learned-behavior reset removes learning data without deleting or corrupting the learning database.
Setup: Record an approved mapping and import a whitelist taxonomy candidate.
Procedure: Clear the learning store, inspect counts, then import a taxonomy candidate again.
Expected outcome: Learned entries, examples, aliases, and embeddings are removed, the database file remains usable, and later imports succeed.
Run: `./build-tests/ai_file_sorter_tests "UserLearningStore clears learned behavior while keeping the database reusable"`

#### Test case: CacheMaintenanceService does not remove learned user behavior with categorization cache
Purpose: Protect user-owned learned behavior from the normal cache-clearing workflow.
Setup: Create both a categorization cache database and a separate user-learning database in a temporary config directory.
Procedure: Clear the categorization cache through `CacheMaintenanceService`.
Expected outcome: The categorization cache is removed or emptied, while `user_learning.db` remains present with the learned example intact.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceService does not remove learned user behavior with categorization cache"`

### `tests/unit/test_cache_maintenance_dialog.cpp` (non-Windows only)

#### Test case: CacheMaintenanceDialog exposes tooltips for each cache target
Purpose: Verify the dialog explains what each cache category means through button tooltips.
Setup: Build the dialog with offscreen Qt and a temporary cache/log location setup.
Procedure: Find each clear button by object name and inspect its tooltip text.
Expected outcome: The categorization, image-location, and log actions all expose a brief explanatory tooltip and remain enabled when the dialog is not busy.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceDialog exposes tooltips for each cache target"`

#### Test case: CacheMaintenanceDialog disables cache clearing controls while busy
Purpose: Ensure the dialog respects the disabled state requested while analysis is running.
Setup: Build the dialog with the busy flag set to true.
Procedure: Inspect the three clear buttons and the busy-state label.
Expected outcome: All clear buttons are disabled and the dialog shows the busy explanation text.
Run: `./build-tests/ai_file_sorter_tests "CacheMaintenanceDialog disables cache clearing controls while busy"`

### `tests/unit/test_utils.cpp`

#### Test case: get_file_name_from_url extracts filename
Purpose: Ensure URL filename extraction returns the last path component.
Setup: Use a URL ending with a file name.
Procedure: Call `Utils::get_file_name_from_url()`.
Expected outcome: The returned string equals the file name (e.g., `mistral-7b.gguf`).
Run: `./build-tests/ai_file_sorter_tests "get_file_name_from_url extracts filename"`

#### Test case: get_file_name_from_url rejects malformed input
Purpose: Confirm invalid URLs are rejected.
Setup: Use a URL with no filename component.
Procedure: Call `Utils::get_file_name_from_url()` and expect an exception.
Expected outcome: A `std::runtime_error` is thrown.
Run: `./build-tests/ai_file_sorter_tests "get_file_name_from_url rejects malformed input"`

#### Test case: is_cuda_available honors probe overrides
Purpose: Verify that CUDA availability probes are honored.
Setup: Install a test hook that returns `true`, then one that returns `false`.
Procedure: Call `Utils::is_cuda_available()` after each probe.
Expected outcome: The function returns `true` and then `false`, matching the probe.
Run: `./build-tests/ai_file_sorter_tests "is_cuda_available honors probe overrides"`

#### Test case: abbreviate_user_path strips home prefix
Purpose: Ensure user paths are shortened relative to `HOME`.
Setup: Create a temporary home directory, set `HOME`, and create a file inside `Documents/`.
Procedure: Call `Utils::abbreviate_user_path()` on the full path.
Expected outcome: The returned string omits the home prefix and begins with `Documents/`.
Run: `./build-tests/ai_file_sorter_tests "abbreviate_user_path strips home prefix"`

#### Test case: sanitize_path_label strips invalid UTF-8 bytes
Purpose: Ensure path labels remain valid UTF-8 even when upstream text contains malformed byte sequences.
Setup: Build a string containing an invalid UTF-8 byte between otherwise valid ASCII text.
Procedure: Call `Utils::sanitize_path_label()`.
Expected outcome: The invalid byte is removed and the returned label remains valid UTF-8 text.
Run: `./build-tests/ai_file_sorter_tests "sanitize_path_label strips invalid UTF-8 bytes"`

#### Test case: Windows Vulkan payload candidates prefer the BLAS runtime layout
Purpose: Ensure Windows runtime lookup prefers the packaged `vulkan-blas` payload before the legacy Vulkan layout.
Setup: Construct a Windows executable path under a representative install root.
Procedure: Call `GgmlRuntimePaths::windows_vulkan_payload_candidate_dirs()`.
Expected outcome: The first candidate is `lib/precompiled/vulkan-blas/bin` and the second is `lib/precompiled/vulkan/bin`.
Run: `./build-tests/ai_file_sorter_tests "Windows Vulkan payload candidates prefer the BLAS runtime layout"`

#### Test case: Windows CPU runtime candidates fall back to the Vulkan runtime layout
Purpose: Ensure launcher-based Windows builds can reuse `wvulkan` when the dedicated CPU runtime directory is absent.
Setup: Construct a Windows executable path under a representative install root.
Procedure: Call `GgmlRuntimePaths::windows_cpu_runtime_candidate_dirs()`.
Expected outcome: The candidate order starts with `wocuda` and then falls back to `wvulkan`.
Run: `./build-tests/ai_file_sorter_tests "Windows CPU runtime candidates fall back to the Vulkan runtime layout"`

#### Test case: Windows CPU runtime resolution falls back to the Vulkan runtime layout
Purpose: Ensure CPU launcher startup can reuse the Vulkan runtime directory when it already contains `ggml-cpu.dll`.
Setup: Create a temporary `lib/ggml/wvulkan` directory containing the required CPU runtime DLL names, without creating `wocuda`.
Procedure: Call `GgmlRuntimePaths::resolve_windows_cpu_runtime_dir()`.
Expected outcome: The resolved path points to `lib/ggml/wvulkan`.
Run: `./build-tests/ai_file_sorter_tests "Windows CPU runtime resolution falls back to the Vulkan runtime layout"`

#### Test case: Windows Vulkan payload resolution prefers the BLAS runtime layout
Purpose: Ensure Windows runtime lookup resolves the BLAS-enabled Vulkan payload when both layouts exist.
Setup: Create temporary `vulkan-blas/bin` and `vulkan/bin` directories containing the required runtime DLL names.
Procedure: Call `GgmlRuntimePaths::resolve_windows_vulkan_payload_dir()`.
Expected outcome: The resolved path points to `lib/precompiled/vulkan-blas/bin`.
Run: `./build-tests/ai_file_sorter_tests "Windows Vulkan payload resolution prefers the BLAS runtime layout"`

#### Test case: Linux Vulkan payload validation rejects stale runtime directories
Purpose: Ensure Linux launch/runtime checks refuse accelerator payloads that contain a backend plugin but omit the versioned core libraries and CPU backend module required by the ggml dynamic-backend layout.
Setup: Create a temporary Vulkan runtime directory with `libggml-vulkan.so` plus only unversioned core library names.
Procedure: Call `GgmlRuntimePaths::validate_linux_accelerator_payload()`.
Expected outcome: Validation fails with a reason that mentions missing Linux runtime dependencies.
Run: `./build-tests/ai_file_sorter_tests "Linux Vulkan payload validation rejects stale runtime directories"`

#### Test case: Linux Vulkan payload validation accepts compatible runtime directories
Purpose: Ensure Linux accelerator payloads remain usable when they contain both the Vulkan plugin and the mix of versioned core libraries and unversioned CPU backend module produced by `GGML_BACKEND_DL=ON`.
Setup: Create a temporary Vulkan runtime directory with `libggml-vulkan.so`, versioned core files such as `libggml.so.0` and `libllama.so.0`, and the CPU backend module `libggml-cpu.so`.
Procedure: Call `GgmlRuntimePaths::validate_linux_accelerator_payload()`.
Expected outcome: Validation succeeds with no rejection reason.
Run: `./build-tests/ai_file_sorter_tests "Linux Vulkan payload validation accepts compatible runtime directories"`

#### Test case: Linux backend environment sanitization demotes stale Vulkan payloads to CPU
Purpose: Ensure the runtime environment is rewritten to CPU when the configured Linux Vulkan payload is stale, preventing the app from advertising or attempting to use an incompatible GPU payload.
Setup: Point `AI_FILE_SORTER_GPU_BACKEND`, `LLAMA_ARG_DEVICE`, and `AI_FILE_SORTER_GGML_DIR` at a temporary stale Vulkan runtime directory.
Procedure: Call `GgmlRuntimePaths::sanitize_linux_backend_environment()`.
Expected outcome: The helper returns a rejection reason, sets `AI_FILE_SORTER_GPU_BACKEND=cpu`, sets `GGML_DISABLE_CUDA=1`, and clears the custom ggml directory/device overrides.
Run: `./build-tests/ai_file_sorter_tests "Linux backend environment sanitization demotes stale Vulkan payloads to CPU"`

#### Test case: sanitize_path_label preserves valid Unicode emoji labels
Purpose: Confirm valid Unicode labels are not stripped while sanitizing invalid path text.
Setup: Build a UTF-8 label containing a cloud emoji.
Procedure: Call `Utils::sanitize_path_label()`.
Expected outcome: The returned label exactly preserves the valid emoji-containing text.
Run: `./build-tests/ai_file_sorter_tests "sanitize_path_label preserves valid Unicode emoji labels"`

#### Test case: format_size keeps byte values in bytes below one kilobyte
Purpose: Ensure small file sizes remain reported in bytes instead of being divided by the kilobyte scale.
Setup: Use one value below 1024 bytes and one value at the kilobyte boundary.
Procedure: Call `Utils::format_size()` for both values.
Expected outcome: `999` formats as `999.00 B` and `1024` formats as `1.00 KB`.
Run: `./build-tests/ai_file_sorter_tests "format_size keeps byte values in bytes below one kilobyte"`

### `tests/unit/test_llm_selection_dialog_local.cpp`

#### Test case: LLM selection dialog lists built-in local models in Gemma Mistral Gemma order
Purpose: Ensure the categorization-model radio buttons present the built-in local models in the intended top-to-bottom order without a separate recommended badge.
Setup: Construct the dialog on a clean config directory and collect the visible built-in local-model labels through test access.
Procedure: Compare the returned labels with the catalog labels for Gemma 4B, Mistral 7B, and Gemma 7B.
Expected outcome: The dialog lists Gemma 4B first, Mistral 7B second, Gemma 7B third, and none of the labels include `Recommended`.
Run: `./build-tests/ai_file_sorter_tests "LLM selection dialog lists built-in local models in Gemma Mistral Gemma order"`

#### Test case: LLM selection dialog defaults to the Gemma 4B local model
Purpose: Verify the built-in local categorization choice defaults to Gemma 4B when no prior selection is stored.
Setup: Construct the dialog with default settings on a clean config directory.
Procedure: Read the selected LLM choice from the dialog.
Expected outcome: The selected built-in choice is `Local_4b_Gemma`.
Run: `./build-tests/ai_file_sorter_tests "LLM selection dialog defaults to the Gemma 4B local model"`

#### Test case: LLM selection dialog keeps the legacy LLaMa choice when the previous Q4 artifact exists
Purpose: Ensure the legacy built-in LLaMa option still appears for users who only have the older pre-Gemma Q4 artifact on disk.
Setup: Write the historical `Llama-3.2-3B-Instruct-bf16-q4_k.gguf` file into the default local LLM cache and preselect `Local_3b_legacy`.
Procedure: Construct the dialog and read back the selected choice.
Expected outcome: The dialog keeps `Local_3b_legacy` selected instead of silently falling back to Gemma 4B.
Run: `./build-tests/ai_file_sorter_tests "LLM selection dialog keeps the legacy LLaMa choice when the previous Q4 artifact exists"`

### `tests/unit/test_llm_selection_dialog_visual.cpp` (non-Windows only)

#### Test case: Visual model entry shows missing env var state
Purpose: Confirm UI indicates missing download URLs for the selected legacy LLaVA visual backend.
Setup: Select the legacy LLaVA backend, clear `LLAVA_MODEL_URL` and `LLAVA_MMPROJ_URL`, and construct the dialog.
Procedure: Fetch the active visual-model entry via test access.
Expected outcome: The status label reports the missing environment variable and the download button is disabled.
Run: `./build-tests/ai_file_sorter_tests "Visual model entry shows missing env var state"`

#### Test case: Visual model entry shows resume state for partial downloads
Purpose: Validate resume state for partial legacy LLaVA visual-model downloads.
Setup: Select the legacy LLaVA backend, create a fake source file and a smaller destination file, and inject metadata headers with an expected size.
Procedure: Update the active visual-model entry state.
Expected outcome: The status label indicates a partial download and the download button changes to "Resume download" and is enabled.
Run: `./build-tests/ai_file_sorter_tests "Visual model entry shows resume state for partial downloads"`

#### Test case: Visual model entry reports download errors
Purpose: Ensure download failures are surfaced in the UI.
Setup: Select the legacy LLaVA backend, inject a network-available override, and inject a download probe that returns a CURL connection error.
Procedure: Start the active visual-model download and wait for the label to update.
Expected outcome: The status label begins with "Download error:" indicating the failure is shown to the user.
Run: `./build-tests/ai_file_sorter_tests "Visual model entry reports download errors"`

#### Test case: Visual backend selection switches descriptor-driven download state
Purpose: Verify the visual download UI is driven by the selected backend descriptor rather than hardcoded LLaVA rows.
Setup: Clear both the default Gemma env vars and the alternate LLaVA env vars, initialize settings with the legacy LLaVA backend, and construct the dialog.
Procedure: Confirm the default backend id and missing-env state, switch the dialog to the Gemma backend through the test access layer, and inspect the active model entry again.
Expected outcome: The selected backend id changes to `gemma-3-4b-it`, and the visible model entry now reports the missing `GEMMA3_4B_MODEL_URL` variable instead of the default backend env var.
Run: `./build-tests/ai_file_sorter_tests "Visual backend selection switches descriptor-driven download state"`

#### Test case: Visual dialog defaults to recommended Gemma backend
Purpose: Verify the visual-model combo defaults to Gemma and marks it as recommended in the download dialog.
Setup: Construct the dialog with default settings on a clean config directory.
Procedure: Read the selected visual backend id and the current combo-box label through the test access layer.
Expected outcome: The selected backend id is `gemma-3-4b-it`, and the visible combo label is `Gemma 3 4B IT (Recommended)`.
Run: `./build-tests/ai_file_sorter_tests "Visual dialog defaults to recommended Gemma backend"`

#### Test case: Visual dialog does not mark another backend's legacy generic mmproj as downloaded
Purpose: Ensure generic legacy mmproj filenames are not attributed to the wrong visual backend.
Setup: Configure a non-LLaVA backend and place a legacy generic mmproj artifact that belongs to a different backend.
Procedure: Build the dialog and inspect the selected backend download state.
Expected outcome: The selected backend does not report complete just because another backend's generic mmproj file exists.
Run: `./build-tests/ai_file_sorter_tests "Visual dialog does not mark another backend's legacy generic mmproj as downloaded"`

#### Test case: Visual dialog accepts the legacy LLaVA generic mmproj without metadata
Purpose: Preserve compatibility with existing LLaVA downloads that predate sidecar metadata.
Setup: Select the legacy LLaVA backend and create the older generic mmproj file without metadata.
Procedure: Build the dialog and inspect the active visual-model entry.
Expected outcome: The LLaVA entry treats the legacy generic mmproj as available.
Run: `./build-tests/ai_file_sorter_tests "Visual dialog accepts the legacy LLaVA generic mmproj without metadata"`

#### Test case: Visual dialog does not mark invalid preferred artifacts as ready
Purpose: Ensure the visual download UI does not treat corrupt backend-specific GGUF files as ready just because they exist on disk.
Setup: Select the Gemma visual backend, create a valid mmproj artifact, and create an invalid model artifact at the preferred backend storage path.
Procedure: Build the dialog and inspect the selected backend entry for the invalid model artifact.
Expected outcome: The entry reports that the downloaded file is invalid or incomplete instead of showing `Model ready.`.
Run: `./build-tests/ai_file_sorter_tests "Visual dialog does not mark invalid preferred artifacts as ready"`

### `tests/unit/test_visual_llm_runtime.cpp`

#### Test case: Default visual model descriptor exposes the MTMD backend catalog
Purpose: Verify the built-in visual model catalog exposes the expected default backend descriptor and the alternate backend entries used by the selector UI.
Setup: Load the default visual model descriptor from the catalog.
Procedure: Inspect the backend id, display name, architecture, prompt policy, required artifact env vars, and the presence of the supported LLaVA Mistral and Gemma backend descriptors.
Expected outcome: The default backend resolves to the Gemma descriptor with separate model and mmproj artifact entries and the structured multimodal prompt policy, while the catalog also exposes `llava-v1.6-mistral-7b`; the disabled Vicuna backend is not registered.
Run: `./build-tests/ai_file_sorter_tests "Default visual model descriptor exposes the MTMD backend catalog"`

#### Test case: VisualLlmRuntime resolves the active backend through descriptor artifacts
Purpose: Ensure runtime resolution goes through the descriptor catalog and can satisfy the mmproj artifact via fallback filenames.
Setup: Set `LLAVA_MODEL_URL` and `LLAVA_MMPROJ_URL`, create the main model file at its default destination, and create a fallback mmproj file in the default LLM directory.
Procedure: Call `resolve_active_backend()` and `resolve_paths()`.
Expected outcome: The backend resolves successfully, returns the LLaVA descriptor, maps the model artifact to the primary file, and maps the mmproj artifact to the fallback file.
Run: `./build-tests/ai_file_sorter_tests "VisualLlmRuntime resolves the active backend through descriptor artifacts"`

#### Test case: VisualLlmRuntime reports missing backend URLs before resolving artifacts
Purpose: Confirm runtime resolution fails early when the configured visual backend is missing its required URL environment variables.
Setup: Clear `LLAVA_MODEL_URL` and `LLAVA_MMPROJ_URL`.
Procedure: Call `resolve_active_backend()` with an error string output.
Expected outcome: Resolution fails and reports the existing missing-URL guidance message.
Run: `./build-tests/ai_file_sorter_tests "VisualLlmRuntime reports missing backend URLs before resolving artifacts"`

#### Test case: VisualLlmRuntime resolves a non-default backend by id
Purpose: Confirm the runtime abstraction can resolve a second visual backend without changing the calling code.
Setup: Set `GEMMA3_4B_MODEL_URL` and `GEMMA3_4B_MMPROJ_URL`, then create the corresponding model and mmproj files at their resolved download destinations.
Procedure: Call `resolve_active_backend("gemma-3-4b-it")`.
Expected outcome: The runtime returns the Gemma descriptor and resolves both artifact paths from the alternate backend env vars.
Run: `./build-tests/ai_file_sorter_tests "VisualLlmRuntime resolves a non-default backend by id"`

#### Test case: VisualLlmRuntime rejects invalid preferred GGUF artifacts
Purpose: Ensure preferred backend artifact paths are only accepted when they contain a valid GGUF header.
Setup: Configure the default Gemma backend and create invalid placeholder files at the preferred model and mmproj storage paths.
Procedure: Call `resolve_active_backend()` with an error string output.
Expected outcome: Resolution fails and reports the preferred model artifact as missing, because invalid GGUF placeholders are ignored.
Run: `./build-tests/ai_file_sorter_tests "VisualLlmRuntime rejects invalid preferred GGUF artifacts"`

### `tests/unit/test_settings_image_options.cpp`

#### Test case: Settings defaults image analysis off even when visual LLM files exist
Purpose: Verify that image analysis defaults remain off when no settings file exists, even if model files are present.
Setup: Create dummy LLaVA model files in the expected location and load settings from an empty config directory.
Procedure: Call `Settings::load()` and read the image analysis flags.
Expected outcome: `load()` returns false, and both analyze and offer-rename flags are false.
Run: `./build-tests/ai_file_sorter_tests "Settings defaults image analysis off even when visual LLM files exist"`

#### Test case: Settings defaults image analysis off when visual LLM files are missing
Purpose: Verify default settings are still off when model files are absent.
Setup: Use a fresh config directory with no LLaVA files.
Procedure: Call `Settings::load()` and read image analysis flags.
Expected outcome: `load()` returns false and analysis/offer-rename remain disabled.
Run: `./build-tests/ai_file_sorter_tests "Settings defaults image analysis off when visual LLM files are missing"`

#### Test case: Settings defaults use subcategories on when config key is missing
Purpose: Ensure the main-window subcategory toggle stays enabled by default when older or partial config files omit the `UseSubcategories` key.
Setup: Create a temporary config file with a `Settings` section that omits `UseSubcategories`.
Procedure: Reload settings from disk.
Expected outcome: `Settings::load()` succeeds and `get_use_subcategories()` returns `true`.
Run: `./build-tests/ai_file_sorter_tests "Settings defaults use subcategories on when config key is missing"`

#### Test case: Settings enforces rename-only implies offer rename
Purpose: Ensure rename-only cannot be enabled without offer-rename.
Setup: Save settings with analyze on, offer-rename off, and rename-only on.
Procedure: Reload settings from disk.
Expected outcome: Offer-rename is forced on while rename-only and process-only settings persist.
Run: `./build-tests/ai_file_sorter_tests "Settings enforces rename-only implies offer rename"`

#### Test case: Settings persists options group expansion state
Purpose: Ensure the image/document option group expansion states persist across load/save.
Setup: Use a temporary config directory and set expanded flags for image and document groups.
Procedure: Save settings, reload into a new `Settings` instance, and read the flags.
Expected outcome: The expansion flags match the saved values.
Run: `./build-tests/ai_file_sorter_tests "Settings persists options group expansion state"`

#### Test case: Settings persists selected visual model backend
Purpose: Ensure the chosen visual backend survives a save/load round-trip so the dialog and runtime stay aligned.
Setup: Use a temporary config directory and set the visual backend id to `gemma-3-4b-it`.
Procedure: Save settings, reload into a new `Settings` instance, and read the stored visual backend id.
Expected outcome: The reloaded settings still report `gemma-3-4b-it`.
Run: `./build-tests/ai_file_sorter_tests "Settings persists selected visual model backend"`

### `tests/unit/test_llava_image_analyzer.cpp`

#### Test case: LlavaImageAnalyzer builds a descending visual GPU-layer retry ladder
Purpose: Ensure visual model-load retries probe smaller `n_gpu_layers` values before giving up on GPU execution.
Setup: Use the visual analyzer test access helper with an initial headroom-aware layer count of `20`.
Procedure: Build the visual retry ladder from the starting layer count.
Expected outcome: The ladder descends as `20, 15, 11, 8, 6, 4, 3, 2, 1`.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer builds a descending visual GPU-layer retry ladder"`

#### Test case: LlavaImageAnalyzer keeps a single visual GPU-layer retry candidate at one layer
Purpose: Avoid redundant retry work once the visual ladder reaches the minimum offload count.
Setup: Use the visual analyzer test access helper with an initial layer count of `1`.
Procedure: Build the visual retry ladder from the starting layer count.
Expected outcome: The ladder contains only `1`.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer keeps a single visual GPU-layer retry candidate at one layer"`

#### Test case: LlavaImageAnalyzer keeps guarded visual projectors on CPU when headroom is tight
Purpose: Verify CUDA and Vulkan visual projector GPU use is gated by available memory headroom.
Setup: Use the visual analyzer test access helper with a representative LLaVA mmproj size and two free-memory levels.
Procedure: Check CUDA and Vulkan with tight memory, CUDA with comfortable memory, and Metal as an unguarded backend.
Expected outcome: CUDA and Vulkan decline projector GPU use when headroom is tight; CUDA accepts it with enough headroom, and Metal remains unchanged.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer keeps guarded visual projectors on CPU when headroom is tight"`

#### Test case: LlavaImageAnalyzer exposes legacy LLaVA prompt policy
Purpose: Verify the legacy visual backends keep the older prompt wording expected by the LLaVA path.
Setup: Use the prompt-policy test access helpers with the legacy policy.
Procedure: Inspect the generated description and filename prompts.
Expected outcome: The description prompt keeps the `Image: <__media__>` / `Description:` shape, and the filename prompt still includes the legacy filename example and trailing `Filename:` cue.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer exposes legacy LLaVA prompt policy"`

#### Test case: LlavaImageAnalyzer exposes structured multimodal prompt policy
Purpose: Verify newer multimodal backends can use a stricter instruction-oriented prompt without changing the runtime abstraction.
Setup: Use the prompt-policy test access helpers with the structured multimodal policy.
Procedure: Inspect the generated description and filename prompts.
Expected outcome: The policy adds explicit system guidance, keeps the media marker in the user prompt, and uses structured filename rules aimed at instruction-tuned backends.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer exposes structured multimodal prompt policy"`

#### Test case: LlavaImageAnalyzer lowers visual ngl when reserving mmproj headroom
Purpose: Ensure the visual GPU layer estimate reserves enough VRAM for the projector and multimodal eval path instead of blindly offloading every text layer that fits.
Setup: Create sparse temporary model files that mirror the logged Gemma visual model and mmproj sizes, then feed the helper the observed 3.7 GiB CUDA memory snapshot.
Procedure: Ask the visual test helper for the mmproj-aware `n_gpu_layers` cap.
Expected outcome: The helper trims the visual GPU layer count down to `20`, leaving headroom for mmproj and MTMD evaluation.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer lowers visual ngl when reserving mmproj headroom"`

#### Test case: LlavaImageAnalyzer reconciles visual ngl to the GPU tier when the reserve-aware estimate is only slightly lower
Purpose: Avoid overly conservative visual caps when the headroom math lands only a few layers below the GPU's overall VRAM tier.
Setup: Create sparse temporary Gemma visual model and mmproj fixtures, then simulate a slightly lower free-memory snapshot on the same 3.7 GiB CUDA card class.
Procedure: Ask the visual test helper for the mmproj-aware `n_gpu_layers` cap with the reduced free-memory value.
Expected outcome: The helper reconciles the result back up to `20` instead of staying at an overly austere lower cap.
Run: `./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer reconciles visual ngl to the GPU tier when the reserve-aware estimate is only slightly lower"`

### `tests/unit/test_checkbox_matrix.cpp`

#### Test case: Checkbox combinations route entries without renamed files
Purpose: Exhaustively validate the file-routing logic for every combination of checkbox flags.
Setup: Define a fixed sample set containing an image, a document, an other file, and a directory. Use an empty set of renamed files.
Procedure: Iterate all 128 combinations of analysis and filtering flags, call `split_entries_for_analysis()`, and compute the expected bucket for each entry.
Expected outcome: Each entry appears only in its expected bucket, image and document buckets contain only supported file types, and a detailed per-combination summary is printed.
Run: `./build-tests/ai_file_sorter_tests "Checkbox combinations route entries without renamed files"`

#### Test case: Checkbox combinations route entries with renamed files
Purpose: Validate routing when image and document entries have already been renamed.
Setup: Use the same sample set but mark the image and document names as already renamed.
Procedure: Repeat the 128-combination sweep and compare actual buckets to expected behavior for rename-only and categorization scenarios.
Expected outcome: Already-renamed items are either skipped or routed to filename-based categorization depending on the rename-only flags, with all entries matching the expected bucket.
Run: `./build-tests/ai_file_sorter_tests "Checkbox combinations route entries with renamed files"`

### `tests/unit/test_llm_downloader.cpp`

#### Test case: LLMDownloader retries full download after a range error
Purpose: Ensure a failed resume attempt triggers a full restart.
Setup: Create a partial destination file and configure the downloader with resume headers. Inject a download probe that returns `CURLE_HTTP_RANGE_ERROR` on the first call and succeeds on the second.
Procedure: Start the download and wait for completion.
Expected outcome: Two attempts are recorded, the second starts from offset 0, the final file size matches the expected size, and no error is reported.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader retries full download after a range error"`

#### Test case: LLMDownloader uses cached metadata for partial downloads
Purpose: Validate that cached metadata drives download status.
Setup: Create a partial local file and an `.aifs.meta` file with the expected content length.
Procedure: Construct the downloader and query its status.
Expected outcome: Both local and overall download status report `InProgress`, content length is read from metadata, and the downloader is not yet initialized.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader uses cached metadata for partial downloads"`

#### Test case: LLMDownloader resets to not started when local file is missing
Purpose: Ensure metadata alone does not imply a partial download.
Setup: Create metadata without the local file.
Procedure: Construct the downloader and query its status.
Expected outcome: The status is `NotStarted` for both local and overall views.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader resets to not started when local file is missing"`

#### Test case: LLMDownloader treats full local file as complete with cached metadata
Purpose: Confirm that a complete local file is recognized as complete.
Setup: Create a local file whose size matches the cached content length.
Procedure: Construct the downloader and query its status.
Expected outcome: Both local and overall download status report `Complete`.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader treats full local file as complete with cached metadata"`

#### Test case: LLMDownloader rejects short completed downloads before replacing the final file
Purpose: Ensure a truncated download cannot overwrite an existing completed model file during finalization.
Setup: Seed a valid final GGUF file, configure the downloader with an expected content length, and inject a probe that writes a one-byte partial file before returning success.
Procedure: Start the download and wait for the error callback.
Expected outcome: Finalization fails with a size-mismatch error, the original final file remains intact, and the short `.part` file is preserved for inspection.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader rejects short completed downloads before replacing the final file"`

#### Test case: LLMDownloader rejects invalid GGUF downloads before replacing the final file
Purpose: Ensure a completed transfer still fails if the resulting `.gguf` payload does not contain a GGUF header.
Setup: Seed a valid final GGUF file, configure the downloader with a matching expected size, and inject a probe that writes same-sized non-GGUF bytes to the partial file before returning success.
Procedure: Start the download and wait for the error callback.
Expected outcome: Finalization fails with an `expected GGUF header` error, the original final file remains intact, and the invalid `.part` file is left on disk.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader rejects invalid GGUF downloads before replacing the final file"`

#### Test case: LLMDownloader does not let callers retarget an active download
Purpose: Prevent live downloads from being rebound to a different model URL while the worker thread is still running.
Setup: Start a download with a blocking test probe so the worker thread remains active.
Procedure: Call `set_download_url()` with a different URL before releasing the probe.
Expected outcome: `set_download_url()` throws, the active download keeps its original URL, and the worker exits cleanly after cancellation.
Run: `./build-tests/ai_file_sorter_tests "LLMDownloader does not let callers retarget an active download"`

### `tests/unit/test_update_feed.cpp`

#### Test case: UpdateFeed selects the correct platform stream
Purpose: Ensure the updater resolves the correct platform-specific stream from the shared feed.
Setup: Build a feed JSON payload with distinct `windows`, `macos`, and `linux` entries.
Procedure: Parse the feed for each platform enum.
Expected outcome: Each platform receives its own version and URLs, and the Windows installer checksum is normalized.
Run: `./build-tests/ai_file_sorter_tests "UpdateFeed selects the correct platform stream"`

#### Test case: UpdateFeed falls back to the legacy single-stream schema
Purpose: Preserve compatibility with existing single-stream feeds.
Setup: Build a feed JSON payload with the original flat `update` object.
Procedure: Parse the feed for a platform.
Expected outcome: The legacy fields are still accepted and returned as update info.
Run: `./build-tests/ai_file_sorter_tests "UpdateFeed falls back to the legacy single-stream schema"`

#### Test case: UpdateFeed normalizes changelog items from text feeds
Purpose: Verify the feed parser accepts a single `changelog` text block and converts bullet-prefixed lines into clean list items.
Setup: Build a legacy single-stream feed whose `changelog` is a multi-line string using `-`, `*`, and `•`.
Procedure: Parse the feed for Linux.
Expected outcome: The changelog is normalized into clean items without the original bullet prefixes.
Run: `./build-tests/ai_file_sorter_tests "UpdateFeed normalizes changelog items from text feeds"`

#### Test case: UpdateInstaller downloads, verifies, and reuses a cached installer
Purpose: Validate the Windows-style installer preparation flow without network access.
Setup: Inject a fake installer download callback and a fake launch callback.
Procedure: Prepare the installer twice and then launch it.
Expected outcome: The first prepare downloads and verifies the installer, the second reuses the cached artifact, and the launch callback receives the finalized path.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller downloads, verifies, and reuses a cached installer"`

#### Test case: UpdateInstaller rejects installers that fail SHA-256 verification
Purpose: Ensure invalid installer downloads are rejected before launch.
Setup: Inject a fake download callback that writes mismatched bytes.
Procedure: Prepare the installer with an expected SHA-256 that does not match.
Expected outcome: Preparation fails and no finalized installer path is returned.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller rejects installers that fail SHA-256 verification"`

#### Test case: UpdateInstaller redownloads cached installers that fail verification
Purpose: Ensure corrupted cached installers are not reused silently.
Setup: Prepare a valid cached installer, then overwrite it with different bytes.
Procedure: Prepare the installer a second time with the same expected SHA-256.
Expected outcome: The cached file is rejected, a new download occurs, and the finalized installer contents match the expected payload.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller redownloads cached installers that fail verification"`

#### Test case: UpdateInstaller reports canceled downloads and removes partial files
Purpose: Confirm cancelation produces a canceled result instead of a generic failure and cleans up partial output.
Setup: Inject a fake download callback that writes a partial file and then throws the installer cancelation exception when the cancel probe is true.
Procedure: Call `prepare()` with a cancel probe that always returns true.
Expected outcome: Preparation returns `Canceled`, no finalized installer path is returned, and the partial `.part` file is removed.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller reports canceled downloads and removes partial files"`

#### Test case: UpdateInstaller requires installer metadata before preparing
Purpose: Reject malformed update feeds that omit required direct-installer fields.
Setup: Create update info once without `installer_url` and once without `installer_sha256`.
Procedure: Call `prepare()` for both cases.
Expected outcome: Both calls fail with messages indicating the missing field.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller requires installer metadata before preparing"`

#### Test case: UpdateInstaller builds launch requests for EXE and MSI installers
Purpose: Verify the installer launch plan uses direct execution for `.exe` files and `msiexec /i` for `.msi` packages.
Setup: Build launch requests for representative `.exe` and `.MSI` paths.
Procedure: Query the test access helper for both inputs.
Expected outcome: The `.exe` request launches the installer directly with no extra arguments, while the `.msi` request targets `msiexec.exe` with `/i <path>`.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller builds launch requests for EXE and MSI installers"`

#### Test case: UpdateInstaller auto-install support remains Windows-only
Purpose: Confirm the direct-installer flow is currently gated to Windows builds.
Setup: Create update info with installer metadata.
Procedure: Query the updater installer support state.
Expected outcome: Windows builds report support; other platforms do not.
Run: `./build-tests/ai_file_sorter_tests "UpdateInstaller auto-install support remains Windows-only"`

### `tests/unit/test_updater.cpp`

#### Test case: Updater optional dialog shows changelog items from the update feed
Purpose: Confirm optional updates display the per-stream changelog list in the dialog before the user makes a choice.
Setup: Construct an updater, attach multiple changelog items to the update info, and auto-click `Cancel`.
Procedure: Open the optional update dialog through updater test access and capture the dialog text.
Expected outcome: The dialog shows the usual prompt plus a bullet list headed by `What's new in version ...`.
Run: `./build-tests/ai_file_sorter_tests "Updater optional dialog shows changelog items from the update feed"`

#### Test case: Updater required dialog shows changelog items before forcing quit
Purpose: Ensure required updates show the same changelog context before the user is forced to update or quit.
Setup: Construct an updater with a fake quit handler, attach changelog items to the update info, and auto-click `Quit`.
Procedure: Open the required update dialog through updater test access and capture the informative text.
Expected outcome: The dialog includes the bullet list for the version and still triggers the quit handler when `Quit` is selected.
Run: `./build-tests/ai_file_sorter_tests "Updater required dialog shows changelog items before forcing quit"`

#### Test case: Updater error dialog offers manual update fallback without quitting when not requested
Purpose: Verify installer-preparation failures still let the user open the normal download page manually for optional updates.
Setup: Construct an updater with test handlers for opening the download URL and quitting the app, and schedule the error dialog to click `Update manually`.
Procedure: Invoke the updater error handler with a `download_url` and `quit_after_open=false`.
Expected outcome: The dialog includes `Update manually`, the download URL handler is called, and the quit handler is not called.
Run: `./build-tests/ai_file_sorter_tests "Updater error dialog offers manual update fallback without quitting when not requested"`

#### Test case: Updater error dialog can request quit after manual fallback
Purpose: Ensure required-update failures can still fall back to the manual download link and then close the app.
Setup: Construct an updater with test handlers and schedule the error dialog to click `Update manually`.
Procedure: Invoke the updater error handler with a `download_url` and `quit_after_open=true`.
Expected outcome: The manual download handler is called and the quit handler is triggered.
Run: `./build-tests/ai_file_sorter_tests "Updater error dialog can request quit after manual fallback"`

#### Test case: Updater error dialog omits manual fallback when no download URL is available
Purpose: Confirm the fallback button is only offered when a manual download link exists.
Setup: Construct an updater with test handlers and invoke the error dialog without a `download_url`.
Procedure: Attempt to click `Update manually`; the helper falls back to `OK` when the button is absent.
Expected outcome: No manual fallback button is present, the error handler returns `false`, and neither the download nor quit handler runs.
Run: `./build-tests/ai_file_sorter_tests "Updater error dialog omits manual fallback when no download URL is available"`

### `tests/unit/test_review_dialog_rename_gate.cpp` (non-Windows only)

#### Test case: Review dialog rename-only toggles disabled when renames are not allowed
Purpose: Verify the review dialog respects the "Offer to rename" gating for images and documents.
Setup: Build a dialog with sample image and document entries and auto-close it using a timer.
Procedure: Show results once with image/document renames disallowed, then again with renames allowed.
Expected outcome: The rename-only checkboxes are disabled in the first case and enabled in the second.
Run: `./build-tests/ai_file_sorter_tests "Review dialog rename-only toggles disabled when renames are not allowed"`

### `tests/unit/test_custom_llm.cpp`

#### Test case: Custom LLM entries persist across Settings load/save
Purpose: Ensure custom LLM definitions persist correctly.
Setup: Insert a custom LLM entry and set it as active, then save settings.
Procedure: Reload settings and retrieve the custom LLM by ID.
Expected outcome: The reloaded entry matches the original fields and the active ID is preserved.
Run: `./build-tests/ai_file_sorter_tests "Custom LLM entries persist across Settings load/save"`

#### Test case: Settings maps legacy Local_3b choices to Gemma 4B
Purpose: Preserve compatibility for older configs that still store the previous built-in 3B identifier.
Setup: Write a minimal config file with `LLMChoice=Local_3b`.
Procedure: Load settings from that config.
Expected outcome: The loaded LLM choice is `Local_4b_Gemma`.
Run: `./build-tests/ai_file_sorter_tests "Settings maps legacy Local_3b choices to Gemma 4B"`

#### Test case: Built-in Gemma 7B choice persists across Settings load/save
Purpose: Ensure the new built-in Gemma 7B choice round-trips through settings persistence.
Setup: Set the LLM choice to `Local_7b_Gemma` and save settings in a clean config directory.
Procedure: Reload settings from disk.
Expected outcome: The reloaded LLM choice remains `Local_7b_Gemma`.
Run: `./build-tests/ai_file_sorter_tests "Built-in Gemma 7B choice persists across Settings load/save"`

#### Test case: Legacy local LLaMa resolves the previous Q4 artifact without marking Gemma 4B ready
Purpose: Preserve compatibility for users who still have the old built-in LLaMa Q4 artifact while preventing that artifact from being mistaken for the new Gemma 4B slot.
Setup: Write the historical `Llama-3.2-3B-Instruct-bf16-q4_k.gguf` file into the default local LLM cache on a clean HOME/config directory.
Procedure: Resolve the downloaded path for `Local_3b_legacy` and query local availability for both the legacy LLaMa and Gemma 4B built-in choices.
Expected outcome: The legacy choice resolves to the old Q4 file, reports available, and the Gemma 4B choice still reports unavailable.
Run: `./build-tests/ai_file_sorter_tests "Legacy local LLaMa resolves the previous Q4 artifact without marking Gemma 4B ready"`

#### Test case: Built-in Gemma 4B resolves the shared visual-model artifact path
Purpose: Ensure the built-in local Gemma choice can reuse the already-downloaded visual Gemma text-model artifact instead of requiring a second copy.
Setup: Configure matching Gemma 4B local/visual download URLs and place only the visual backend's preferred model artifact under the backend-specific storage directory.
Procedure: Resolve the downloaded path for `Local_4b_Gemma` and query built-in availability.
Expected outcome: The built-in Gemma choice resolves to the visual backend artifact path and reports available.
Run: `./build-tests/ai_file_sorter_tests "Built-in Gemma 4B resolves the shared visual-model artifact path"`

### `tests/unit/test_category_language_support.cpp`

#### Test case: Gemma 3 category language support exposes the full language list
Purpose: Ensure the built-in Gemma 3 4B local model keeps the full expanded category-language catalog available.
Setup: Query the category-language support helper for `Local_4b_Gemma`.
Procedure: Compare the returned list against the app-wide category-language list and spot-check the alphabetical first/last entries.
Expected outcome: The Gemma 3 helper returns the full category-language set, starting at `Afrikaans` and ending at `Zulu`.
Run: `./build-tests/ai_file_sorter_tests "Gemma 3 category language support exposes the full language list"`

#### Test case: Custom local models keep the full category language list
Purpose: Ensure user-supplied local models are not restricted more aggressively than the built-in Gemma 3 baseline.
Setup: Query the category-language support helper for `Custom`.
Procedure: Compare the returned list against the app-wide category-language list.
Expected outcome: The custom-local helper returns the full category-language set.
Run: `./build-tests/ai_file_sorter_tests "Custom local models keep the full category language list"`

#### Test case: Mistral 7B category language support stays limited to the supported subset
Purpose: Keep the built-in Mistral 7B local model limited to the conservative translation-language subset discussed for the app.
Setup: Query the category-language support helper for `Local_7b`.
Procedure: Verify supported and unsupported languages individually and confirm the expected list length.
Expected outcome: English, Dutch, French, German, Italian, Swedish, Norwegian, Danish, and Spanish are supported; the rest of the app's category-language options are not.
Run: `./build-tests/ai_file_sorter_tests "Mistral 7B category language support stays limited to the supported subset"`

#### Test case: English-only built-in local models expose only English
Purpose: Ensure the legacy 3B local model and built-in Gemma 1.1 7B local model do not advertise unsupported non-English category-language translation.
Setup: Query the category-language support helper for `Local_3b_legacy` and `Local_7b_Gemma`.
Procedure: Inspect the returned lists.
Expected outcome: Both helpers return a one-entry list containing only English.
Run: `./build-tests/ai_file_sorter_tests "English-only built-in local models expose only English"`

### `tests/unit/test_database_manager_rename_only.cpp`

#### Test case: DatabaseManager keeps rename-only entries with empty labels
Purpose: Ensure rename-only entries are not removed when categories are empty.
Setup: Insert one rename-only entry with a suggested name and one empty entry with no rename suggestion.
Procedure: Call `remove_empty_categorizations()` and then fetch categorized files.
Expected outcome: Only the truly empty entry is removed; the rename-only entry remains with empty category labels and the suggestion intact.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager keeps rename-only entries with empty labels"`

#### Test case: DatabaseManager keeps suggestion-only entries with empty labels
Purpose: Ensure entries that only contain a suggested filename are retained for later review.
Setup: Insert a suggestion-only entry with empty category labels and another truly empty entry.
Procedure: Call `remove_empty_categorizations()` and then fetch categorized files.
Expected outcome: The suggestion-only row remains, while the truly empty row is removed.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager keeps suggestion-only entries with empty labels"`

#### Test case: DatabaseManager sanitizes invalid UTF-8 in cached labels
Purpose: Ensure malformed UTF-8 in cached category labels or suggestions does not propagate into the review dialog pipeline.
Setup: Insert a cached entry whose category, subcategory, and suggested filename contain invalid UTF-8 bytes.
Procedure: Fetch categorized files from the database.
Expected outcome: The loaded category, subcategory, and suggested name are returned with invalid UTF-8 bytes removed.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager sanitizes invalid UTF-8 in cached labels"`

#### Test case: DatabaseManager normalizes subcategory stopword suffixes for taxonomy matching
Purpose: Verify taxonomy resolution normalizes stopword suffixes like "files".
Setup: Resolve categories with and without the "files" suffix (e.g., "Graphics" vs "Graphics files").
Procedure: Compare the resolved taxonomy IDs and labels.
Expected outcome: Both resolutions share the same taxonomy ID and normalized labels, while unrelated subcategories (e.g., "Photos") remain unchanged.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes subcategory stopword suffixes for taxonomy matching"`

#### Test case: DatabaseManager preserves the Backups family under archive-like labels
Purpose: Ensure archive-like labels resolve to a stable backups semantic family instead of being flattened into generic archives.
Setup: Resolve `Archives` and `backup files` with the same subcategory.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: Both labels map to the same taxonomy entry with canonical category `Backups`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager preserves the Backups family under archive-like labels"`

#### Test case: DatabaseManager normalizes image category synonyms and image media aliases
Purpose: Ensure image-related category variants collapse while non-image media remains distinct.
Setup: Resolve `Images`, `Graphics`, `Media + Photos`, and `Media + Audio`.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: `Images/Graphics/Media+Photos` share taxonomy and canonicalize to `Images`; `Media+Audio` remains `Media`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes image category synonyms and image media aliases"`

#### Test case: DatabaseManager canonicalizes legacy Music categories into Audio
Purpose: Ensure the legacy `Music` main category resolves to the newer `Audio` bucket without changing the leaf subcategory.
Setup: Resolve both `Audio + Podcast` and `Music + Podcast`.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: Both labels resolve to the same taxonomy entry and canonicalize to `Audio / Podcast`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager canonicalizes legacy Music categories into Audio"`

#### Test case: DatabaseManager canonicalizes Installer Builders subcategories into Installer Tools
Purpose: Ensure the ambiguous legacy installer-builder label resolves to the clearer `Installer Tools` subcategory.
Setup: Resolve both `Software + Installer Tools` and `Software + Installer Builders`.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: Both labels resolve to the same taxonomy entry and canonicalize to `Software / Installer Tools`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager canonicalizes Installer Builders subcategories into Installer Tools"`

#### Test case: DatabaseManager normalizes document category synonyms for taxonomy matching
Purpose: Ensure document-like category variants collapse to `Documents`.
Setup: Resolve `Documents`, `Texts`, `Papers`, and `Spreadsheets` with the same subcategory.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: All variants map to the same taxonomy entry with canonical category `Documents`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes document category synonyms for taxonomy matching"`

#### Test case: DatabaseManager normalizes generic Documents labels into preserved document families when the subcategory is explicit
Purpose: Keep specialized document-family labels consistent even when the model returns generic `Documents` as the category.
Setup: Resolve generic `Documents` labels with explicit subcategories such as manuals, spreadsheets, and presentations.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: The resolved category preserves the intended semantic family, such as `Manuals`, `Spreadsheets`, or `Presentations`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes generic Documents labels into preserved document families when the subcategory is explicit"`

#### Test case: DatabaseManager keeps specialized document-family categories and normalizes generic subcategories
Purpose: Preserve specialized document categories while cleaning generic or redundant subcategory labels.
Setup: Resolve specialized categories such as `Manuals` and `Spreadsheets` with generic subcategory text.
Procedure: Compare resolved taxonomy labels.
Expected outcome: Specialized categories remain specialized, and generic subcategory values are normalized to useful labels.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager keeps specialized document-family categories and normalizes generic subcategories"`

#### Test case: DatabaseManager structurally canonicalizes broad main labels without reclassifying software semantics
Purpose: Canonicalize broad family labels like `Installer` and `Operating System` without semantically reclassifying `Software / Installers` into a different main category.
Setup: Resolve broad family labels and a mixed `Software / Installers` pair.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: Broad family labels normalize to `Installers` or `Operating Systems` with `General` when repeated, while `Software / Installers` stays `Software / Installers`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager structurally canonicalizes broad main labels without reclassifying software semantics"`

#### Test case: DatabaseManager keeps non-family software semantics under Software
Purpose: Ensure software labels that are not installer/update semantics remain generic software.
Setup: Resolve non-installer software categories and subcategories.
Procedure: Compare resolved taxonomy labels.
Expected outcome: Non-family software semantics remain under canonical category `Software`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager keeps non-family software semantics under Software"`

#### Test case: DatabaseManager can clear cached categorizations together with taxonomy state
Purpose: Ensure the categorization reset path can remove both cached file rows and taxonomy pollution.
Setup: Create a taxonomy entry, store a cached file categorization, and keep the temporary database path.
Procedure: Call `clear_all_categorizations(true)` and inspect the SQLite tables directly.
Expected outcome: `file_categorization`, `category_taxonomy`, `category_alias`, and `category_translation` are all empty afterward.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager can clear cached categorizations together with taxonomy state"`

#### Test case: DatabaseManager migrates legacy audio and installer-builder taxonomy labels on reopen
Purpose: Ensure old cached DB rows using `Music` or `Installer Builders` are upgraded automatically when the app reopens.
Setup: Seed a temporary SQLite cache with legacy taxonomy rows and cached file rows that still use those labels.
Procedure: Reopen `DatabaseManager`, fetch the cached files, and inspect the resulting taxonomy table.
Expected outcome: Cached files reload as `Audio / Podcast` and `Software / Installer Tools`, and no legacy `music` or `installer builders` taxonomy rows remain.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager migrates legacy audio and installer-builder taxonomy labels on reopen"`

### `tests/unit/test_file_scanner.cpp`

#### Test case: hidden files require explicit flag
Purpose: Ensure hidden files are filtered unless explicitly requested.
Setup: Create a hidden file in a temporary directory.
Procedure: Scan with only `Files`, then with `Files | HiddenFiles`.
Expected outcome: The hidden file is absent in the first scan and present in the second.
Run: `./build-tests/ai_file_sorter_tests "hidden files require explicit flag"`

#### Test case: junk files are skipped regardless of flags
Purpose: Confirm that known junk files are always excluded.
Setup: Create a `.DS_Store` file.
Procedure: Scan with `Files | HiddenFiles`.
Expected outcome: The junk file does not appear in the results.
Run: `./build-tests/ai_file_sorter_tests "junk files are skipped regardless of flags"`

#### Test case: application bundles are treated as files
Purpose: Ensure application bundles are treated as files rather than directories.
Setup: Create a `Sample.app` directory with a `Contents` subdirectory.
Procedure: Scan once for files and once for directories.
Expected outcome: The bundle appears only in the file scan and not in the directory scan.
Run: `./build-tests/ai_file_sorter_tests "application bundles are treated as files"`

#### Test case: recursive scans include nested files
Purpose: Ensure recursive scans still return files from nested subdirectories.
Setup: Create one file in the root and one file in a nested subdirectory.
Procedure: Scan with `Files | Recursive`.
Expected outcome: Both files appear in the results.
Run: `./build-tests/ai_file_sorter_tests "recursive scans include nested files"`

#### Test case: recursive scans skip unreadable directories and continue
Purpose: Ensure one inaccessible subdirectory does not abort an otherwise valid recursive scan.
Setup: Create a readable subtree and a second subtree whose directory permissions are removed (non-Windows only).
Procedure: Scan with `Files | Recursive`.
Expected outcome: The readable file is returned, the scan does not throw, and the unreadable subtree is skipped.
Run: `./build-tests/ai_file_sorter_tests "recursive scans skip unreadable directories and continue"`

### `tests/unit/test_support_prompt.cpp`

#### Test case: Support prompt thresholds advance based on response
Purpose: Verify support prompt scheduling logic under different user responses.
Setup: Create a fresh settings environment and define a callback that returns a simulated response (`NotSure`, `CannotDonate`, or `Support`).
Procedure: Increment the categorized file count to the current threshold, observe the prompt, then advance to the next threshold.
Expected outcome: The prompt fires exactly at thresholds, the total count increments correctly, and the next threshold increases for all response types.
Run: `./build-tests/ai_file_sorter_tests "Support prompt thresholds advance based on response"`

#### Test case: Zero categorized increments do not change totals or trigger prompts
Purpose: Ensure a zero increment is a no-op.
Setup: Fresh settings with a baseline threshold.
Procedure: Call the prompt simulation with an increment of `0`.
Expected outcome: Total counts and thresholds remain unchanged and the callback is not invoked.
Run: `./build-tests/ai_file_sorter_tests "Zero categorized increments do not change totals or trigger prompts"`

### `tests/unit/test_custom_api_endpoint.cpp`

#### Test case: Custom API endpoints persist across Settings load/save
Purpose: Ensure custom OpenAI-compatible endpoint definitions persist correctly.
Setup: Create a custom endpoint with name, description, base URL, API key, and model, then set it active and save.
Procedure: Reload settings and retrieve the endpoint by ID.
Expected outcome: All fields match the original, and the active endpoint ID is preserved.
Run: `./build-tests/ai_file_sorter_tests "Custom API endpoints persist across Settings load/save"`

### `tests/unit/test_categorization_dialog.cpp` (non-Windows only)

#### Test case: CategorizationDialog uses subcategory toggle when moving files
Purpose: Ensure the dialog respects the subcategory visibility toggle during file moves.
Setup: Create a sample categorized file and attach a move probe.
Procedure: Toggle the subcategory column state and confirm the dialog.
Expected outcome: The move probe records the same subcategory setting that was applied.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog uses subcategory toggle when moving files"`

#### Test case: CategorizationDialog supports sorting by columns
Purpose: Verify that the table model sorts correctly by different columns.
Setup: Insert two entries with out-of-order file names and categories.
Procedure: Sort by the file name column ascending, then by category descending.
Expected outcome: The first sort yields alphabetical file names; the second yields categories in reverse alphabetical order.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog supports sorting by columns"`

#### Test case: CategorizationDialog undo restores moved files
Purpose: Confirm that undo reverses category moves.
Setup: Create a file on disk with a category and subcategory.
Procedure: Confirm the dialog to move the file, then trigger undo.
Expected outcome: The file moves to the category path, then returns to the original location; undo is enabled only when a move exists.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog undo restores moved files"`

#### Test case: CategorizationDialog undo allows renaming again
Purpose: Ensure undo resets rename-only operations and allows reapplication.
Setup: Create a rename-only entry with a suggested name.
Procedure: Confirm the rename, undo it, and confirm again.
Expected outcome: Each confirm applies the rename, and undo restores the original filename for a second rename.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog undo allows renaming again"`

#### Test case: CategorizationDialog dry run logs preview completion without moved success
Purpose: Prevent dry-run sessions from reporting that files were actually moved.
Setup: Enable the dialog's dry-run checkbox for a categorized file while routing logs into an isolated cache directory.
Procedure: Confirm the dialog, auto-close the preview popup, and inspect `core.log`.
Expected outcome: The source file stays in place, no destination file is created, and the log contains the dry-run completion message without the real-move success message.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog dry run logs preview completion without moved success"`

#### Test case: UndoManager restores saved plans through the active storage provider
Purpose: Verify persisted undo plans can be replayed through the storage provider abstraction.
Setup: Create a saved move plan and attach a storage provider that records undo calls.
Procedure: Load and execute the undo plan.
Expected outcome: The provider receives the expected restore request and the plan is consumed successfully.
Run: `./build-tests/ai_file_sorter_tests "UndoManager restores saved plans through the active storage provider"`

#### Test case: UndoManager relaxes timestamp validation for cloud providers
Purpose: Allow cloud-backed providers to restore moves when timestamp metadata is less reliable.
Setup: Create a saved cloud-provider move plan with timestamp differences that would fail strict local validation.
Procedure: Execute the undo flow through the active provider.
Expected outcome: The undo succeeds despite timestamp drift because provider identity metadata is trusted.
Run: `./build-tests/ai_file_sorter_tests "UndoManager relaxes timestamp validation for cloud providers"`

#### Test case: CategorizationDialog rename-only updates cached filename
Purpose: Verify database updates when a rename-only action occurs.
Setup: Use a dialog with a database manager and a rename-only file with a suggestion.
Procedure: Confirm the dialog and query the database.
Expected outcome: The old name is not cached; the new name is cached with rename-only metadata and the suggested name.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog rename-only updates cached filename"`

#### Test case: CategorizationDialog allows editing when rename-only checkbox is off
Purpose: Ensure category fields remain editable when rename-only mode is not enforced.
Setup: Populate the dialog with one rename-only entry and one categorized entry.
Procedure: Inspect the category column editability in the model.
Expected outcome: Both rows remain editable in the category column.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog allows editing when rename-only checkbox is off"`

#### Test case: CategorizationDialog deduplicates suggested names when rename-only is toggled
Purpose: Ensure duplicate suggestions are made unique when rename-only is turned on.
Setup: Provide two image entries with identical suggested names.
Procedure: Toggle the rename-only checkbox in the dialog.
Expected outcome: The suggestions are rewritten with numbered suffixes (e.g., `_1`, `_2`).
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog deduplicates suggested names when rename-only is toggled"`

#### Test case: CategorizationDialog avoids double suffixes for numbered suggestions
Purpose: Prevent double-numbering when suggestions already contain a suffix.
Setup: Use two rename-only entries with a suggestion ending in `_1`.
Procedure: Populate the dialog and read back the suggested names.
Expected outcome: The first remains `_1`, and the second becomes `_2` without duplicating the suffix.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog avoids double suffixes for numbered suggestions"`

#### Test case: CategorizationDialog hides suggested names for renamed entries
Purpose: Hide rename suggestions once the rename has already been applied.
Setup: Create an entry with `rename_applied=true` and a suggested name.
Procedure: Populate the dialog and inspect the suggested name cell.
Expected outcome: The suggested name cell is empty and not editable.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog hides suggested names for renamed entries"`

#### Test case: CategorizationDialog hides already renamed rows when rename-only is on
Purpose: Ensure completed renames are hidden when only renaming is requested.
Setup: Add one renamed entry and one pending entry, then enable the rename-only checkbox.
Procedure: Toggle rename-only and inspect row visibility.
Expected outcome: The already renamed row becomes hidden while the pending row remains visible.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog hides already renamed rows when rename-only is on"`

#### Test case: CategorizationDialog deduplicates suggested picture filenames
Purpose: Ensure image rename suggestions are unique across multiple rows.
Setup: Provide two rename-only image entries with identical suggested names.
Procedure: Populate the dialog and read the suggested names.
Expected outcome: Suggestions become `_1` and `_2` variants to avoid collisions.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog deduplicates suggested picture filenames"`

#### Test case: CategorizationDialog avoids existing picture filename collisions
Purpose: Ensure suggested names do not collide with existing files on disk.
Setup: Create a file on disk that matches the suggested name and add a rename-only entry with that suggestion.
Procedure: Populate the dialog and inspect the suggestion.
Expected outcome: The suggestion is incremented (e.g., `_1`) to avoid the existing file.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog avoids existing picture filename collisions"`

#### Test case: CategorizationDialog rename-only preserves cached categories without renaming
Purpose: Ensure rename-only mode keeps existing category assignments even when no rename occurs.
Setup: Cache a categorization in the database, then run the dialog with a rename-only entry that has no suggested name.
Procedure: Confirm the dialog and query the cache.
Expected outcome: The cached category and subcategory are preserved and the entry remains marked rename-only.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog rename-only preserves cached categories without renaming"`

#### Test case: CategorizationDialog rename-only preserves cached categories when renaming
Purpose: Ensure rename-only mode keeps cached categories after a rename.
Setup: Cache a categorization, then run the dialog with a rename-only entry that includes a suggested name.
Procedure: Confirm the dialog and query the cache for the renamed file.
Expected outcome: The renamed entry retains the cached category and subcategory with rename-only metadata.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog rename-only preserves cached categories when renaming"`

#### Test case: CategorizationDialog records confirmed categories as learned behavior
Purpose: Ensure user-approved review decisions are copied into the dedicated learning store.
Setup: Create a dialog with a categorization cache, a user-learning store, and a confirmed categorized file.
Procedure: Trigger confirmation and inspect the learning database.
Expected outcome: The approved category/subcategory and analysis context are stored as learned behavior with the file example attached.
Run: `./build-tests/ai_file_sorter_tests "CategorizationDialog records confirmed categories as learned behavior"`

### `tests/unit/test_main_app_translation.cpp` (non-Windows only)

#### Test case: MainApp retranslate reflects language changes
Purpose: Validate that main window labels update for all supported UI languages.
Setup: Construct `MainApp` with a settings object and a translation manager.
Procedure: Iterate through supported languages, set the language, trigger a retranslate, and read the analyze button and folder label text.
Expected outcome: Each language produces the exact expected translations for the two labels.
This now explicitly includes Hindi, Swedish, Icelandic, Norwegian, Finnish, Danish, and Simplified Chinese.
Run: `./build-tests/ai_file_sorter_tests "MainApp retranslate reflects language changes"`

#### Test case: Top-level menu titles are translated for all supported UI languages
Purpose: Verify the main menu-bar labels are covered by the translation catalogs for every shipped interface language.
Setup: Iterate all supported UI languages through the translation manager.
Procedure: Translate the `UiTranslator` menu titles for File, Edit, View, Settings, Plugins, Development, Tests, Interface language, and Category language through `QCoreApplication::translate(...)`.
Expected outcome: Each supported language returns the exact expected localized menu labels, including the Hindi entries, the newly added Nordic UI languages, and Simplified Chinese.
Run: `./build-tests/ai_file_sorter_tests "Top-level menu titles are translated for all supported UI languages"`

#### Test case: Settings menu actions are translated for all supported UI languages
Purpose: Verify the visible Settings-menu action labels are localized rather than falling back to English source text.
Setup: Iterate all supported UI languages through the translation manager.
Procedure: Translate `System compatibility check…`, `Select &LLM…`, `Manage category whitelists…`, `Interface &language`, `Category &language`, `Reset learned behavior…`, and `Clear cache…` through `QCoreApplication::translate(...)`.
Expected outcome: Each supported language returns the expected localized action labels, including the Hindi entries, the newly added Nordic UI languages, and Simplified Chinese shown in the Settings menu.
Run: `./build-tests/ai_file_sorter_tests "Settings menu actions are translated for all supported UI languages"`

#### Test case: Updater strings are translated for all supported UI languages
Purpose: Verify updater and installer UI text exists across supported languages.
Setup: Iterate the supported UI languages through the translation manager.
Procedure: Read translated updater labels, errors, progress strings, and changelog headings.
Expected outcome: Each supported language returns the expected localized updater strings.
This now explicitly includes Hindi, Swedish, Icelandic, Norwegian, Finnish, Danish, and Simplified Chinese.
Run: `./build-tests/ai_file_sorter_tests "Updater strings are translated for all supported UI languages"`

#### Test case: Quick Start guide content follows the selected app language
Purpose: Ensure the local Quick Start guide body follows the active app language.
Setup: Set the translation manager to English, French, Korean, Hindi, Swedish, Icelandic, Norwegian, Finnish, Danish, and Simplified Chinese.
Procedure: Resolve the Quick Start markdown for each selected language.
Expected outcome: Each language loads the matching localized markdown content instead of the English fallback when a translation exists.
Run: `./build-tests/ai_file_sorter_tests "Quick Start guide content follows the selected app language"`

#### Test case: Interface language action labels are translated for the newly added Nordic UI languages
Purpose: Verify the new interface-language menu entries (`&Swedish`, `&Icelandic`, `&Norwegian`, `&Finnish`, and `&Danish`) are present in the translation catalogs and render localized labels across every supported UI language.
Setup: Iterate all supported UI languages through the translation manager.
Procedure: Translate the five new interface-language action labels through the `UiTranslator` context.
Expected outcome: Every supported UI language returns the exact expected localized menu labels for all five new language actions.
Run: `./build-tests/ai_file_sorter_tests "Interface language action labels are translated for the newly added Nordic UI languages"`

#### Test case: Simplified Chinese interface language action label is translated for all supported UI languages
Purpose: Verify the `&Simplified Chinese` interface-language menu entry is present in every shipped translation catalog and does not fall back to English.
Setup: Iterate all supported UI languages through the translation manager.
Procedure: Translate `&Simplified Chinese` through the `UiTranslator` context.
Expected outcome: Every supported UI language returns the exact expected localized label for the Simplified Chinese menu action.
Run: `./build-tests/ai_file_sorter_tests "Simplified Chinese interface language action label is translated for all supported UI languages"`

#### Test case: Interface language labels keep localized capitalization style
Purpose: Prevent mixed-case language-name menus by checking the locales that intentionally use title case and the locales that intentionally use lowercase language names.
Setup: Iterate the affected supported UI languages through the translation manager.
Procedure: Translate the specific outlier language labels that previously mixed title case and lowercase in the same menu.
Expected outcome: Each checked locale returns the exact expected capitalization for those interface-language labels.
Run: `./build-tests/ai_file_sorter_tests "Interface language labels keep localized capitalization style"`

#### Test case: Quick Start and FAQ help labels are translated for all supported UI languages
Purpose: Verify Help menu labels and the Quick Start dialog title are covered by translation catalogs.
Setup: Iterate all supported UI languages.
Procedure: Translate `&Quick Start Guide`, `&FAQ`, and `Quick Start Guide` through Qt translation contexts.
Expected outcome: Each supported language returns the expected localized labels.
Run: `./build-tests/ai_file_sorter_tests "Quick Start and FAQ help labels are translated for all supported UI languages"`

### `tests/run_translation_tests.sh`

#### Translation catalog sync check
Purpose: Ensure the committed `.ts` catalogs do not miss any currently used GUI source strings.
Setup: Create temporary copies of all supported UI language catalogs, including Hindi, Swedish, Icelandic, Norwegian, Finnish, Danish, and Simplified Chinese, and locate Qt 6 `lupdate`/`lrelease`.
Procedure: Run `lupdate` against `app/startapp_windows.cpp`, `app/lib/*.cpp`, and `app/include/*.hpp`, then scan the temporary catalogs for newly introduced `unfinished` entries.
Expected outcome: No temporary catalog contains unfinished entries, which means the source tree and translation catalogs are in sync.
Run: `./tests/run_translation_tests.sh`

### `tests/unit/test_whitelist_and_prompt.cpp`

#### Test case: WhitelistStore seeds built-in presets when empty
Purpose: Ensure a new whitelist store starts with the built-in presets.
Setup: Create a temporary settings store without saved whitelists.
Procedure: Initialize `WhitelistStore` and read the available entries.
Expected outcome: Built-in presets are present and selectable, and the default categories include `Audio` rather than the legacy `Music` label.
Run: `./build-tests/ai_file_sorter_tests "WhitelistStore seeds built-in presets when empty"`

#### Test case: WhitelistStore migrates the Documents preset once for legacy stores
Purpose: Verify legacy saved whitelist data receives the updated Documents preset without repeated migration.
Setup: Create a legacy whitelist settings payload.
Procedure: Initialize the store twice and inspect the stored presets.
Expected outcome: The Documents preset is migrated once and remains stable on the second load.
Run: `./build-tests/ai_file_sorter_tests "WhitelistStore migrates the Documents preset once for legacy stores"`

#### Test case: WhitelistStore initializes from settings and persists defaults
Purpose: Ensure whitelist entries are loaded into settings and persisted.
Setup: Create a whitelist entry, save it, and initialize the store from settings with a selected whitelist name.
Procedure: Verify the settings fields and reload the whitelist store from disk.
Expected outcome: The whitelist name, categories, and subcategories remain consistent across initialization and reload.
Run: `./build-tests/ai_file_sorter_tests "WhitelistStore initializes from settings and persists defaults"`

#### Test case: WhitelistStore migrates legacy Music categories to Audio
Purpose: Ensure persisted whitelist entries upgrade the legacy `Music` category to the canonical `Audio` bucket.
Setup: Seed a temporary whitelist file with a semicolon-delimited default preset containing `Music`, `Videos`, and `Audio`.
Procedure: Load the whitelist store, then reload it from disk.
Expected outcome: The stored default categories become `Audio` and `Videos` without duplicate audio-family entries.
Run: `./build-tests/ai_file_sorter_tests "WhitelistStore migrates legacy Music categories to Audio"`

#### Test case: WhitelistStore preserves Unicode labels through save and load
Purpose: Ensure valid Unicode whitelist labels, including emoji, survive persistence.
Setup: Save a whitelist entry containing Unicode category/subcategory labels.
Procedure: Reload the whitelist store from settings.
Expected outcome: The Unicode labels are unchanged after the round trip.
Run: `./build-tests/ai_file_sorter_tests "WhitelistStore preserves Unicode labels through save and load"`

#### Test case: CategorizationService builds numbered whitelist context
Purpose: Confirm the whitelist context includes numbered categories and an "any" subcategory fallback.
Setup: Set allowed categories in settings and build a service instance.
Procedure: Call the test access method to build the whitelist context string.
Expected outcome: The context includes numbered category lines and indicates that subcategories are unrestricted.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService builds numbered whitelist context"`

#### Test case: CategorizationService preserves Unicode whitelist labels in combined context
Purpose: Ensure Unicode whitelist labels are forwarded into model prompt context.
Setup: Configure a whitelist containing Unicode labels and build a categorization service.
Procedure: Build the combined whitelist/category-language context.
Expected outcome: The generated context preserves the Unicode labels exactly.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService preserves Unicode whitelist labels in combined context"`

#### Test case: CategorizationService keeps small whitelists fully injected
Purpose: Preserve existing predictable prompt behavior for small whitelists.
Setup: Configure a small whitelist with two categories and build a categorization service.
Procedure: Build the combined prompt context for a matching file.
Expected outcome: The full numbered whitelist is included and no large-whitelist candidate block is used.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService keeps small whitelists fully injected"`

#### Test case: CategorizationService retrieves candidates instead of injecting large whitelists
Purpose: Prevent large whitelists from being dumped into the LLM prompt.
Setup: Configure a large whitelist, seed matching whitelist candidates in the learning store, and attach the store to the service.
Procedure: Build the combined prompt context for a matching file.
Expected outcome: The context contains a compact large-whitelist candidate block with the relevant category, omits unrelated whitelist entries, and does not duplicate the generic learned-candidate block.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService retrieves candidates instead of injecting large whitelists"`

#### Test case: CategorizationService ranks large whitelist candidates without learning store
Purpose: Ensure large-whitelist prompt reduction still works when the learning database has not been seeded.
Setup: Configure a large whitelist with one lexically matching category and no learning store.
Procedure: Build the combined prompt context for a matching file.
Expected outcome: The context uses the large-whitelist candidate block and includes the lexically matched category without injecting unrelated entries.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService ranks large whitelist candidates without learning store"`

#### Test case: CategorizationService adds relevant learned taxonomy candidates to context
Purpose: Ensure learned behavior produces a small prompt candidate block before LLM categorization.
Setup: Record a review-confirmed manual mapping, import an unrelated spreadsheet candidate, and build a categorization service with the learning store attached.
Procedure: Build combined context for a camera manual file.
Expected outcome: The context includes the learned candidate normalized into the stable document family as `Documents : Camera Guides`.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService adds relevant learned taxonomy candidates to context"`

#### Test case: CategorizationService prefers learned candidates over generic model categories
Purpose: Prefer a strong user-learned category when the model returns a generic category for a semantically matching file.
Setup: Record a review-confirmed manual mapping and use an LLM stub that returns `Documents : General`.
Procedure: Categorize a camera manual file through the service.
Expected outcome: The final category/subcategory uses the learned `Documents : Camera Guides` mapping instead of the generic model output.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService prefers learned candidates over generic model categories"`

#### Test case: CategorizationService ignores whitelist-imported candidates when the model already returns a specific document subcategory
Purpose: Prevent whitelist-derived taxonomy candidates from overriding an already specific model result for document-like files.
Setup: Import a whitelist-sourced taxonomy candidate that lexically matches a receipt file and use an LLM stub that returns `Documents : Receipts`.
Procedure: Categorize the receipt file through the service with the learning store attached.
Expected outcome: The final category/subcategory remains `Documents : Receipts` instead of being replaced by the whitelist-imported candidate.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService ignores whitelist-imported candidates when the model already returns a specific document subcategory"`

#### Test case: CategorizationService builds category language context when non-English selected
Purpose: Ensure the category language context is generated for non-English settings.
Setup: Set the category language to French.
Procedure: Build the category language context string.
Expected outcome: The context is non-empty and references "French".
Run: `./build-tests/ai_file_sorter_tests "CategorizationService builds category language context when non-English selected"`

#### Test case: CategorizationService builds category language context for Spanish
Purpose: Verify Spanish category language is handled explicitly.
Setup: Set the category language to Spanish.
Procedure: Build the category language context string.
Expected outcome: The context is non-empty and references "Spanish".
Run: `./build-tests/ai_file_sorter_tests "CategorizationService builds category language context for Spanish"`

#### Test case: LocalLLM sanitizer keeps labeled multi-line replies intact
Purpose: Preserve valid labeled category/subcategory responses while removing unrelated text.
Setup: Provide a multi-line response with category and subcategory labels.
Procedure: Run the local LLM response sanitizer.
Expected outcome: The labeled category/subcategory answer remains parseable.
Run: `./build-tests/ai_file_sorter_tests "LocalLLM sanitizer keeps labeled multi-line replies intact"`

#### Test case: LocalLLM sanitizer prefers the last inline pair
Purpose: Handle model responses that include multiple inline category pairs.
Setup: Provide a response with more than one candidate pair.
Procedure: Run the sanitizer.
Expected outcome: The last inline pair is retained for parsing.
Run: `./build-tests/ai_file_sorter_tests "LocalLLM sanitizer prefers the last inline pair"`

#### Test case: LocalLLM sanitizer strips rationale and natural language lead-ins
Purpose: Remove explanatory lead-in text before category labels.
Setup: Provide a response with natural-language rationale plus a valid pair.
Procedure: Run the sanitizer.
Expected outcome: The resulting text contains only the parseable category/subcategory answer.
Run: `./build-tests/ai_file_sorter_tests "LocalLLM sanitizer strips rationale and natural language lead-ins"`

#### Test case: LocalLLM sanitizer ignores trailing note lines
Purpose: Remove extra notes after a valid category response.
Setup: Provide a valid response followed by a note.
Procedure: Run the sanitizer.
Expected outcome: The trailing note is omitted.
Run: `./build-tests/ai_file_sorter_tests "LocalLLM sanitizer ignores trailing note lines"`

#### Test case: LocalLLM sanitizer strips translated parenthetical glosses
Purpose: Keep canonical labels clean when the model adds translated explanations in parentheses.
Setup: Provide category labels with parenthetical glosses.
Procedure: Run the sanitizer.
Expected outcome: Parenthetical glosses are removed from the parseable answer.
Run: `./build-tests/ai_file_sorter_tests "LocalLLM sanitizer strips translated parenthetical glosses"`

#### Test case: LocalLLM sanitizer strips inline subcategory label artifacts from category values
Purpose: Prevent duplicated label artifacts from contaminating category values.
Setup: Provide a response where `Subcategory:` appears inline after the category value.
Procedure: Run the sanitizer.
Expected outcome: The category value excludes the inline subcategory label artifact.
Run: `./build-tests/ai_file_sorter_tests "LocalLLM sanitizer strips inline subcategory label artifacts from category values"`

#### Test case: CategorizationService parses category output without spaced colon delimiters
Purpose: Ensure category parsing accepts compact `Category:Subcategory` output.
Setup: Use a fixed LLM stub response `Documents:Invoices`.
Procedure: Run `categorize_entries` for one file entry.
Expected outcome: Parsed category is `Documents` and parsed subcategory is `Invoices`.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService parses category output without spaced colon delimiters"`

#### Test case: CategorizationService parses labeled category and subcategory lines
Purpose: Ensure category parsing accepts labeled multiline output.
Setup: Use a fixed LLM stub response with `Category: ...` and `Subcategory: ...` lines.
Procedure: Run `categorize_entries` for one file entry.
Expected outcome: Parsed labels match the provided category and subcategory values.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService parses labeled category and subcategory lines"`

#### Test case: CategorizationService extracts the trailing pair from verbose responses
Purpose: Recover the final category/subcategory pair when the model includes verbose text first.
Setup: Provide a verbose response ending with a valid pair.
Procedure: Run categorization and parse the result.
Expected outcome: The trailing valid pair is used.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService extracts the trailing pair from verbose responses"`

#### Test case: CategorizationService prefers the final pair when the model echoes examples
Purpose: Ignore echoed examples before the model's final answer.
Setup: Provide a response containing example pairs followed by a final pair.
Procedure: Run categorization and parse the result.
Expected outcome: The final answer pair is selected.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService prefers the final pair when the model echoes examples"`

#### Test case: CategorizationService strips rationale from subcategory text
Purpose: Clean subcategory values when a model appends explanatory rationale.
Setup: Provide a subcategory answer followed by rationale text.
Procedure: Run categorization and parse the result.
Expected outcome: The parsed subcategory excludes the rationale.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService strips rationale from subcategory text"`

#### Test case: CategorizationService extracts a short category from natural language lead-ins
Purpose: Accept concise category answers buried in natural-language lead-ins.
Setup: Provide a response with prose followed by a short category/subcategory pair.
Procedure: Run categorization and parse the result.
Expected outcome: The short category/subcategory pair is extracted.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService extracts a short category from natural language lead-ins"`

#### Test case: CategorizationService ignores trailing note lines after a valid answer
Purpose: Avoid adding model notes to parsed labels.
Setup: Provide a valid category/subcategory answer followed by note lines.
Procedure: Run categorization and parse the result.
Expected outcome: The parsed labels exclude trailing notes.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService ignores trailing note lines after a valid answer"`

#### Test case: CategorizationService progress shows current and categorization paths
Purpose: Ensure progress messages identify both the original path and categorization target.
Setup: Run categorization with a progress callback.
Procedure: Capture progress messages emitted for a file.
Expected outcome: The messages include the current file path and categorization path context.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService progress shows current and categorization paths"`

#### Test case: Document prompt helpers use the suggested filename for categorization
Purpose: Ensure document prompts prefer the AI-suggested filename when present.
Setup: Provide an original filename and a suggested filename.
Procedure: Resolve the document prompt name through the helper.
Expected outcome: The suggested filename is used.
Run: `./build-tests/ai_file_sorter_tests "Document prompt helpers use the suggested filename for categorization"`

#### Test case: Document prompt path uses the suggested filename and preserves summaries
Purpose: Ensure document prompt paths combine suggested filenames with extracted summaries.
Setup: Provide a document path, suggested filename, and summary text.
Procedure: Build the document prompt path through the test access helper.
Expected outcome: The prompt path uses the suggested filename and retains the summary payload.
Run: `./build-tests/ai_file_sorter_tests "Document prompt path uses the suggested filename and preserves summaries"`

#### Test case: Image prompt path uses the suggested filename and preserves descriptions
Purpose: Ensure image categorization prompts use the suggested filename while preserving the visual description payload.
Setup: Create a temporary image path, a suggested filename, and a sample visual description.
Procedure: Build the image prompt path through the test access helper.
Expected outcome: The generated prompt path starts with the suggested filename path, includes the `Image description:` suffix, and omits the legacy filename.
Run: `./build-tests/ai_file_sorter_tests "Image prompt path uses the suggested filename and preserves descriptions"`

#### Test case: CategorizationService passes image descriptions through prompt overrides
Purpose: Verify that image prompt overrides forward the visual description payload to the categorization LLM.
Setup: Create a categorization service with a prompt-capturing LLM stub, then prepare an image entry with a suggested filename and generated image prompt path.
Procedure: Run `categorize_entries` with a prompt override for the image entry and inspect the captured prompt path passed to the LLM.
Expected outcome: The captured prompt contains the suggested filename and `Image description:` section, and omits the original filename.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService passes image descriptions through prompt overrides"`

#### Test case: CategorizationService preserves analysis context for learned behavior capture
Purpose: Carry document summary or image description text through categorization so approved review mappings can store richer learning context.
Setup: Build a document prompt path containing a summary and categorize with a stub model.
Procedure: Inspect the resulting `CategorizedFile`.
Expected outcome: The categorized file exposes the extracted analysis context for later storage by the review dialog.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService preserves analysis context for learned behavior capture"`

#### Test case: CategorizationService adds stable guidance for supported document prompts
Purpose: Ensure both analyzable and legacy Office document prompts receive shared document-specific guidance and the bounded document main-category candidate list before the LLM runs.
Setup: Build a document prompt path for a representative `.pdf` file with a summary and use a plain path for a representative legacy `.doc` file.
Procedure: Generate the combined prompt context through the categorization service test access layer for both files.
Expected outcome: Both contexts include document guidance that emphasizes stable main categories and subject-focused subcategories, plus the ordered `Documents`/`Presentations`/`Spreadsheets`/`Data Exports`/`Configs` main-category list.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService adds stable guidance for supported document prompts"`

#### Test case: CategorizationService normalizes supported document main categories to stable buckets
Purpose: Prevent supported document files from fragmenting across topical main categories such as `Security`, `Marketing`, or `Computing`.
Setup: Prepare representative `.pdf`, `.pptx`, `.xlsx`, `.csv`, `.conf`, `.doc`, `.xls`, and `.ppt` prompt overrides with stubbed LLM responses that use unstable topical main categories.
Procedure: Categorize each entry through the service and inspect the canonical result.
Expected outcome: The final main categories normalize to the bounded set `Documents`, `Presentations`, `Spreadsheets`, `Data Exports`, and `Configs` while preserving the subject matter in the subcategory.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService normalizes supported document main categories to stable buckets"`

#### Test case: CategorizationService preserves explicit whitelist document main categories
Purpose: Avoid breaking workflows where a document whitelist intentionally uses subject-specific main categories.
Setup: Enable a whitelist whose allowed main categories include `Contracts`, then prepare a summarized `.pdf` prompt override and a matching stub response.
Procedure: Categorize the entry through the service.
Expected outcome: The whitelist-approved `Contracts` main category is preserved instead of being normalized back to `Documents`.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService preserves explicit whitelist document main categories"`

#### Test case: CategorizationService adds stable guidance for supported image prompts
Purpose: Ensure supported image files receive shared image-specific guidance and an `Images`-only main-category candidate list before the LLM runs, even without a generated visual description payload.
Setup: Build a plain prompt path for a representative `.jpg` file.
Procedure: Generate the combined prompt context through the categorization service test access layer.
Expected outcome: The context includes image guidance that keeps `Images` as the main category and pushes the depicted subject into the subcategory.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService adds stable guidance for supported image prompts"`

#### Test case: CategorizationService leaves generic software-like prompts unscaffolded without whitelist
Purpose: Keep non-document, non-image prompts close to the older minimal prompt style when no whitelist is active.
Setup: Build a prompt context for a representative `.exe` file with default settings.
Procedure: Generate the combined prompt context through the categorization service test access layer.
Expected outcome: The combined context is empty, meaning no extra software-family scaffolding was injected.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService leaves generic software-like prompts unscaffolded without whitelist"`

#### Test case: CategorizationService adds artifact guidance for software-like prompts only when whitelist mode is active
Purpose: Preserve targeted artifact guidance for installer/software files when whitelist mode is enabled.
Setup: Enable a whitelist with software-family categories and build a prompt context for a representative `.exe` file.
Procedure: Generate the combined prompt context through the categorization service test access layer.
Expected outcome: The context includes the software/archive artifact guidance plus whitelist-driven allowed main categories, and uses the clearer `Installer Tools` label.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService adds artifact guidance for software-like prompts only when whitelist mode is active"`

#### Test case: CategorizationService structurally canonicalizes artifact family labels without semantic remapping
Purpose: Keep only structural cleanup for artifact files, such as singular/plural main-category canonicalization and repeated-family fallback to `General`.
Setup: Prepare representative `.exe` files and stub LLM responses that use broad family labels like `Installer`, `Driver`, and `Operating Systems`.
Procedure: Categorize each entry through the service and inspect the canonical result.
Expected outcome: The service canonicalizes broad family labels structurally, but does not remap specific artifact semantics into different main categories.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService structurally canonicalizes artifact family labels without semantic remapping"`

#### Test case: CategorizationService preserves non-family artifact semantics from the model output
Purpose: Avoid post-processing that rewrites specific software/archive semantics into different categories after the model reply is parsed.
Setup: Prepare representative `.zip` files and stub LLM responses that use specific non-family categories such as `Database Management Tools`, `System Utilities`, and `Analytics`.
Procedure: Categorize each entry through the service and inspect the canonical result.
Expected outcome: The final categories preserve those non-family labels, while broad family labels like `Archive` are still canonicalized structurally.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService preserves non-family artifact semantics from the model output"`

#### Test case: CategorizationService falls back to General when a broad family subcategory repeats the category
Purpose: Prefer a usable `Category / General` result over dropping the categorization when the model repeats a broad family label in both positions.
Setup: Prepare a representative audio file and use an LLM stub that returns `Audio : Audio`.
Procedure: Categorize the file through the service.
Expected outcome: The final category/subcategory becomes `Audio / General` instead of an uncategorized fallback.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService falls back to General when a broad family subcategory repeats the category"`

#### Test case: CategorizationService preserves explicit whitelist software-like main categories
Purpose: Avoid overriding user-curated whitelist mains for software-like or archive-like files.
Setup: Enable a whitelist with a non-family category such as `Utilities`, then prepare a representative software/archive file and matching stub response.
Procedure: Categorize the entry through the service.
Expected outcome: The whitelist-approved main category is preserved instead of being normalized back into the bounded software/archive family.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService preserves explicit whitelist software-like main categories"`

#### Test case: CategorizationService leaves unknown generic prompts unscaffolded without whitelist
Purpose: Keep unknown-extension generic files on the minimal prompt path when no specialized image/document guidance applies.
Setup: Build a prompt context for a representative unknown-extension file.
Procedure: Generate the combined prompt context through the categorization service test access layer.
Expected outcome: The combined context is empty, meaning the service did not inject fallback category scaffolding.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService leaves unknown generic prompts unscaffolded without whitelist"`

#### Test case: CategorizationService uses one-shot categorization prompts for document files
Purpose: Keep document categorization on the simpler single-pass prompt path while preserving the newer main-category guidance and normalization afterward.
Setup: Prepare a summarized `.pdf` prompt override and a prompt-capturing LLM stub that returns `Documents : PCI DSS`.
Procedure: Categorize the file through the service and inspect the captured categorization path and combined context.
Expected outcome: The service performs one categorization call, the document summary stays attached to the prompt path, the combined context still includes document guidance plus allowed main categories, and no split-pass-only subcategory prompt text is present.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService uses one-shot categorization prompts for document files"`

#### Test case: CategorizationService normalizes supported image main categories to Images
Purpose: Prevent supported image files from fragmenting across topical main categories such as `Wildlife`, `Paris_Skyline`, or screenshot subject headings.
Setup: Prepare representative `.png` and `.jpg` prompt overrides, with and without generated image descriptions, and stub LLM responses that use unstable topical main categories.
Procedure: Categorize each entry through the service and inspect the canonical result.
Expected outcome: The final main category normalizes to `Images` while preserving the depicted subject in the subcategory.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService normalizes supported image main categories to Images"`

#### Test case: CategorizationService preserves explicit whitelist image main categories when Images is disallowed
Purpose: Avoid breaking image whitelists that intentionally exclude `Images` as an allowed main category.
Setup: Enable a whitelist whose allowed main categories exclude `Images`, then prepare a rich image prompt override and a matching stub response.
Procedure: Categorize the entry through the service.
Expected outcome: The whitelist-approved non-`Images` main category is preserved instead of being normalized to `Images`.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService preserves explicit whitelist image main categories when Images is disallowed"`

#### Test case: CategorizationService adds subject-focused guidance for screenshot-like image prompts
Purpose: Improve categorization prompts for screenshots and UI-like image descriptions.
Setup: Build an image prompt path from a screenshot-like visual description.
Procedure: Generate the categorization prompt.
Expected outcome: The prompt includes stable `Images` main-category guidance and subject-focused guidance for screenshot-like content.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService adds subject-focused guidance for screenshot-like image prompts"`

#### Test case: CategorizationService skips extension-only consistency hints for rich image prompts
Purpose: Avoid weak extension-only hints when image descriptions already provide rich visual context.
Setup: Build a rich image prompt and consistency hints based only on extension.
Procedure: Generate categorization guidance.
Expected outcome: Extension-only hints are omitted for the rich image prompt.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService skips extension-only consistency hints for rich image prompts"`

#### Test case: CategorizationService stores canonical English labels and persists translated taxonomy labels
Purpose: Preserve canonical taxonomy storage while displaying translated labels.
Setup: Categorize with a non-English category language.
Procedure: Store and reload the categorization result.
Expected outcome: Canonical English labels are stored, and translated taxonomy labels are persisted for display.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService stores canonical English labels and persists translated taxonomy labels"`

#### Test case: CategorizationService strips inline subcategory label artifacts when parsing service output
Purpose: Ensure service-level parsing removes inline `Subcategory:` artifacts from category text.
Setup: Provide categorization output with inline label artifacts.
Procedure: Run service parsing.
Expected outcome: Parsed category and subcategory values are clean.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService strips inline subcategory label artifacts when parsing service output"`

### `tests/unit/test_cache_interactions.cpp`

#### Test case: CategorizationService uses cached categorization without calling LLM
Purpose: Ensure cached category/subcategory rows are returned without invoking the LLM.
Setup: Seed the database with a resolved category for a file entry and prepare a counting LLM stub.
Procedure: Call `categorize_entries` for that file.
Expected outcome: The cached category is returned and the LLM call counter stays at zero.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService uses cached categorization without calling LLM"`

#### Test case: CategorizationService falls back to LLM when cache is empty
Purpose: Validate cache fallback to LLM and persistence of returned category values.
Setup: Seed an empty cache record for a file and prepare a counting LLM stub with a valid label response.
Procedure: Call `categorize_entries` for that file and then read the DB row.
Expected outcome: The LLM is called once and the resulting category/subcategory are written back to cache.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService falls back to LLM when cache is empty"`

#### Test case: CategorizationService loads cached entries recursively for analysis
Purpose: Confirm recursive cache loading obeys the `include_subdirectories` setting.
Setup: Seed one cached row at root level and one in a child path.
Procedure: Call `load_cached_entries` with recursion off, then on.
Expected outcome: Non-recursive mode returns only root rows; recursive mode returns both rows.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService loads cached entries recursively for analysis"`

#### Test case: ResultsCoordinator respects full-path cache keys for recursive scans
Purpose: Ensure recursive scans treat same-name files in different folders as distinct when using full-path cache keys.
Setup: Create duplicate filenames at root and nested paths, then seed cache keys by full path.
Procedure: Compute uncached entries via `find_files_to_categorize`.
Expected outcome: Only the truly uncached nested path remains in the result set.
Run: `./build-tests/ai_file_sorter_tests "ResultsCoordinator respects full-path cache keys for recursive scans"`

#### Test case: CategorizationService invokes completion callback per entry
Purpose: Verify per-entry completion notifications fire for categorization progress tracking.
Setup: Prepare multiple file entries and callbacks that count queued/completed events.
Procedure: Run `categorize_entries` and capture callback counters.
Expected outcome: Queue and completion callbacks are each invoked once per processed entry.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService invokes completion callback per entry"`

#### Test case: StoragePluginManager refreshes available plugins from a remote catalog
Purpose: Confirm remote catalog refresh merges plugin metadata for the current runtime.
Setup: Point the manager at a mock remote catalog URL with a runtime-matching plugin manifest.
Procedure: Call `refresh_remote_catalog` and inspect the available plugin list afterward.
Expected outcome: The refreshed catalog entry appears in the available plugin list and is eligible for install/update checks.
Run: `./build-tests/ai_file_sorter_tests "StoragePluginManager refreshes available plugins from a remote catalog"`

#### Test case: StoragePluginManager reports when a remote catalog lacks a matching runtime
Purpose: Ensure remote update checks fail with a precise runtime-mismatch message instead of a generic fetch failure.
Setup: Point the manager at a mock remote catalog URL whose only plugin entry targets a different platform and architecture.
Procedure: Call `refresh_remote_catalog` and capture the returned error string.
Expected outcome: Refresh fails and reports that the catalog does not contain any entries for the current runtime.
Run: `./build-tests/ai_file_sorter_tests "StoragePluginManager reports when a remote catalog lacks a matching runtime"`

#### Test case: StoragePluginManager installs catalog plugins on demand
Purpose: Verify remote catalog entries can be downloaded and installed when the user selects them.
Setup: Provide a mock catalog entry plus mock remote manifest and archive downloads.
Procedure: Call `install` for the catalog-delivered plugin id.
Expected outcome: The plugin installs successfully and its managed manifest/package artifacts are written to disk.
Run: `./build-tests/ai_file_sorter_tests "StoragePluginManager installs catalog plugins on demand"`

#### Test case: OneDriveStorageProvider prefers authoritative sync-root detection when available
Purpose: Ensure OneDrive detection uses authoritative sync-root information ahead of heuristic path matching.
Setup: Inject a sync-root resolver that reports a OneDrive provider for the selected root.
Procedure: Call `detect` on a matching root path.
Expected outcome: Detection succeeds with the authoritative source and the provider resolves as OneDrive.
Run: `./build-tests/ai_file_sorter_tests "OneDriveStorageProvider prefers authoritative sync-root detection when available"`

#### Test case: OneDriveStorageProvider rejects heuristic matches when authoritative sync-root detection reports a different provider
Purpose: Prevent false positives when Windows reports a different cloud provider for the selected root.
Setup: Inject a sync-root resolver that reports a non-OneDrive provider for a path whose name still looks like OneDrive.
Procedure: Call `detect` on that path.
Expected outcome: The authoritative non-OneDrive result vetoes the heuristic match.
Run: `./build-tests/ai_file_sorter_tests "OneDriveStorageProvider rejects heuristic matches when authoritative sync-root detection reports a different provider"`

#### Test case: OneDriveStorageProvider owns undo moves and cleans empty folders
Purpose: Verify OneDrive move/undo operations are handled by the provider itself rather than delegated to the local provider.
Setup: Create a source file and destination folder tree in a temporary directory.
Procedure: Move the file through `move_entry`, then restore it with `undo_move`.
Expected outcome: The file returns to its original location and provider-created empty directories are removed.
Run: `./build-tests/ai_file_sorter_tests "OneDriveStorageProvider owns undo moves and cleans empty folders"`

### `tests/unit/test_main_app_storage_support.cpp`

#### Test case: MainApp resolves installable storage support when plugin is not installed
Purpose: Ensure detected cloud folders map to the correct installable plugin when support exists but is not installed yet.
Setup: Build `MainApp` with a clean config directory and synthesize Dropbox detection metadata.
Procedure: Resolve the storage-support state and plugin id via the test-access helpers.
Expected outcome: The state is `detected_but_plugin_not_installed` and the plugin id resolves to `cloud_storage_compat`.
Run: `./build-tests/ai_file_sorter_tests "MainApp resolves installable storage support when plugin is not installed"`

#### Test case: MainApp resolves plugin-backed storage support when plugin is installed
Purpose: Confirm detected cloud folders resolve to the installed plugin-backed mode when support is already present.
Setup: Install `cloud_storage_compat`, then build `MainApp` with Dropbox detection metadata.
Procedure: Resolve the storage-support state and plugin id via the test-access helpers.
Expected outcome: The state is `detected_and_supported_via_plugin` and the plugin id resolves to `cloud_storage_compat`.
Run: `./build-tests/ai_file_sorter_tests "MainApp resolves plugin-backed storage support when plugin is installed"`

#### Test case: MainApp resolves unsupported storage support when no plugin exists
Purpose: Distinguish cloud providers that are detected heuristically but have no matching plugin support.
Setup: Build `MainApp` and synthesize detection metadata for an unsupported provider such as Google Drive.
Procedure: Resolve the storage-support state and plugin id via the test-access helpers.
Expected outcome: The state is `detected_but_no_plugin_exists` and no plugin id is returned.
Run: `./build-tests/ai_file_sorter_tests "MainApp resolves unsupported storage support when no plugin exists"`

### `tests/unit/test_storage_plugin_dialog.cpp`

#### Test case: StoragePluginDialog refreshes plugin metadata on open and shows update actions
Purpose: Ensure opening the plugin dialog refreshes remote metadata in the background and exposes per-plugin update actions.
Setup: Install a mock external-process plugin locally and provide a mock remote catalog entry advertising a newer version.
Procedure: Construct the dialog, pump events until the background refresh completes, and inspect the plugin list row.
Expected outcome: The installed plugin becomes updateable and its row shows an `Update` action button.
Run: `./build-tests/ai_file_sorter_tests "StoragePluginDialog refreshes plugin metadata on open and shows update actions"`

### Test infrastructure: `tests/unit/test_cli_reporter.cpp`

This file registers a Catch2 event listener that prints a one-line "[TEST]" banner for each test case as it begins. It does not define test cases itself, but it makes CLI output easier to follow during long runs.
