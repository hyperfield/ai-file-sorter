# Test Suite Guide

This document provides a detailed description of every test case in the project. It is organized by test file and mirrors the intent, setup, procedure, and expected outcomes for each case. All unit tests live under `tests/unit`. Some UI-centric tests are compiled only on non-Windows platforms and use the Qt offscreen platform plugin so they can run without a visible display.

## How to run tests
- Configure tests (once): `cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON`
- Build and run all tests: `cmake --build build-tests` then `ctest --test-dir build-tests --output-on-failure -j $(nproc)`
- Run a single test case by name: `./build-tests/ai_file_sorter_tests "<test case name or pattern>"`
- MediaInfo is expected from a package manager (`apt`/`dnf`/`pacman`/`brew`/`vcpkg`); vendored MediaInfo directories/binaries are intentionally rejected by the build.

## Unit test catalog

### `tests/unit/test_local_llm_backend.cpp` (skipped when `GGML_USE_METAL` is defined)

#### Test case: detect_preferred_backend reads environment
Purpose: Verify that the backend preference resolver honors the explicit environment override.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND` to `cuda` via an environment guard.
Procedure: Call `detect_preferred_backend()` through the test access layer.
Expected outcome: The detected preference is `Cuda`.
Run: `./build-tests/ai_file_sorter_tests "detect_preferred_backend reads environment"`

#### Test case: CPU backend is honored when forced
Purpose: Ensure the GPU layer count is forced to CPU when the backend is set to CPU.
Setup: Create a temporary GGUF model file and set `AI_FILE_SORTER_GPU_BACKEND=cpu`. Ensure no CUDA disable flag or layer override is set.
Procedure: Call `prepare_model_params_for_testing()` for the temporary model.
Expected outcome: `n_gpu_layers` is `0`.
Run: `./build-tests/ai_file_sorter_tests "CPU backend is honored when forced"`

#### Test case: CUDA backend can be forced off via GGML_DISABLE_CUDA
Purpose: Confirm that the global CUDA disable flag overrides a CUDA backend preference.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda` and `GGML_DISABLE_CUDA=1`. Inject a probe that reports CUDA available.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` is `0`, indicating CPU fallback.
Run: `./build-tests/ai_file_sorter_tests "CUDA backend can be forced off via GGML_DISABLE_CUDA"`

#### Test case: CUDA override is applied when backend is available
Purpose: Validate that an explicit layer override is used when CUDA is available.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, set `AI_FILE_SORTER_N_GPU_LAYERS=7`, and inject a CUDA-available probe.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` equals `7`.
Run: `./build-tests/ai_file_sorter_tests "CUDA override is applied when backend is available"`

#### Test case: CUDA fallback when no GPU is available
Purpose: Ensure CUDA preference falls back when no GPU is detected.
Setup: Set `AI_FILE_SORTER_GPU_BACKEND=cuda`, leave layer override unset, and inject a CUDA-unavailable probe.
Procedure: Call `prepare_model_params_for_testing()`.
Expected outcome: `n_gpu_layers` is `0` or `-1` (CPU or auto fallback).
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

### `tests/unit/test_main_app_image_options.cpp` (non-Windows only)

#### Test case: Image analysis checkboxes enable and enforce rename-only behavior
Purpose: Ensure the image analysis options enable correctly and enforce the rename-only rule.
Setup: Create dummy LLaVA model files, configure settings with image analysis and rename options off, and construct `MainApp` with offscreen Qt.
Procedure: Toggle the "Analyze picture files" checkbox on, then toggle the "Do not categorize picture files" checkbox on and attempt to unset "Offer to rename picture files".
Expected outcome: The option group enables when analysis is checked; enabling rename-only forces offer-rename on; disabling offer-rename clears rename-only.
Run: `./build-tests/ai_file_sorter_tests "Image analysis checkboxes enable and enforce rename-only behavior"`

#### Test case: Image rename-only does not disable categorization unless processing images only
Purpose: Confirm that rename-only for images does not disable file categorization by itself.
Setup: Initialize settings with image analysis off and build `MainApp` with offscreen Qt.
Procedure: Enable image analysis and rename-only, then check whether "Categorize files" remains enabled. Next, enable "Process picture files only".
Expected outcome: Categorization remains enabled with rename-only, but becomes disabled when processing images only.
Run: `./build-tests/ai_file_sorter_tests "Image rename-only does not disable categorization unless processing images only"`

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

### `tests/unit/test_ui_translator.cpp` (non-Windows only)

#### Test case: UiTranslator updates menus, actions, and controls
Purpose: Validate that the UI translator updates all primary controls, menus, and stateful labels in a consistent pass.
Setup: Build a test harness with a `QMainWindow`, many UI controls, and a translator state set to French in settings. Use a translation function that returns the input string to test label wiring rather than actual translation files.
Procedure: Call `retranslate_all()` and verify the text of buttons, checkboxes, menus, status labels, and the file explorer dock title. Also verify the language action group selection.
Expected outcome: All UI elements show the expected English strings and the French language action is marked checked, demonstrating the retranslate pipeline is correctly wired.
Run: `./build-tests/ai_file_sorter_tests "*UiTranslator updates menus*"`

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

### `tests/unit/test_llm_selection_dialog_visual.cpp` (non-Windows only)

#### Test case: Visual LLaVA entry shows missing env var state
Purpose: Confirm UI indicates missing LLaVA download URLs.
Setup: Clear `LLAVA_MODEL_URL` and `LLAVA_MMPROJ_URL` and construct the dialog.
Procedure: Fetch the LLaVA entry via test access.
Expected outcome: The status label reports the missing environment variable and the download button is disabled.
Run: `./build-tests/ai_file_sorter_tests "Visual LLaVA entry shows missing env var state"`

#### Test case: Visual LLaVA entry shows resume state for partial downloads
Purpose: Validate resume state for partial LLaVA downloads.
Setup: Create a fake source file and a smaller destination file, inject metadata headers with an expected size.
Procedure: Update the LLaVA entry state.
Expected outcome: The status label indicates a partial download and the download button changes to "Resume download" and is enabled.
Run: `./build-tests/ai_file_sorter_tests "Visual LLaVA entry shows resume state for partial downloads"`

#### Test case: Visual LLaVA entry reports download errors
Purpose: Ensure download failures are surfaced in the UI.
Setup: Inject a network-available override and a download probe that returns a CURL connection error.
Procedure: Start the LLaVA model download and wait for the label to update.
Expected outcome: The status label begins with "Download error:" indicating the failure is shown to the user.
Run: `./build-tests/ai_file_sorter_tests "Visual LLaVA entry reports download errors"`

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

### `tests/unit/test_database_manager_rename_only.cpp`

#### Test case: DatabaseManager keeps rename-only entries with empty labels
Purpose: Ensure rename-only entries are not removed when categories are empty.
Setup: Insert one rename-only entry with a suggested name and one empty entry with no rename suggestion.
Procedure: Call `remove_empty_categorizations()` and then fetch categorized files.
Expected outcome: Only the truly empty entry is removed; the rename-only entry remains with empty category labels and the suggestion intact.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager keeps rename-only entries with empty labels"`

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

#### Test case: DatabaseManager normalizes backup category synonyms for taxonomy matching
Purpose: Ensure backup/archive category variants collapse to a single canonical taxonomy entry.
Setup: Resolve `Archives` and `backup files` with the same subcategory.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: Both labels map to the same taxonomy entry with canonical category `Archives`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes backup category synonyms for taxonomy matching"`

#### Test case: DatabaseManager normalizes image category synonyms and image media aliases
Purpose: Ensure image-related category variants collapse while non-image media remains distinct.
Setup: Resolve `Images`, `Graphics`, `Media + Photos`, and `Media + Audio`.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: `Images/Graphics/Media+Photos` share taxonomy and canonicalize to `Images`; `Media+Audio` remains `Media`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes image category synonyms and image media aliases"`

#### Test case: DatabaseManager normalizes document category synonyms for taxonomy matching
Purpose: Ensure document-like category variants collapse to `Documents`.
Setup: Resolve `Documents`, `Texts`, `Papers`, and `Spreadsheets` with the same subcategory.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: All variants map to the same taxonomy entry with canonical category `Documents`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes document category synonyms for taxonomy matching"`

#### Test case: DatabaseManager normalizes installer and update category synonyms for taxonomy matching
Purpose: Ensure software/install/update category variants collapse to `Software`.
Setup: Resolve `Software`, `Installers`, `Setup files`, `Software Update`, and `Patches`.
Procedure: Compare taxonomy IDs and canonical labels.
Expected outcome: All variants map to the same taxonomy entry with canonical category `Software`.
Run: `./build-tests/ai_file_sorter_tests "DatabaseManager normalizes installer and update category synonyms for taxonomy matching"`

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

### `tests/unit/test_main_app_translation.cpp` (non-Windows only)

#### Test case: MainApp retranslate reflects language changes
Purpose: Validate that main window labels update for all supported UI languages.
Setup: Construct `MainApp` with a settings object and a translation manager.
Procedure: Iterate through supported languages, set the language, trigger a retranslate, and read the analyze button and folder label text.
Expected outcome: Each language produces the exact expected translations for the two labels.
Run: `./build-tests/ai_file_sorter_tests "MainApp retranslate reflects language changes"`

### `tests/unit/test_whitelist_and_prompt.cpp`

#### Test case: WhitelistStore initializes from settings and persists defaults
Purpose: Ensure whitelist entries are loaded into settings and persisted.
Setup: Create a whitelist entry, save it, and initialize the store from settings with a selected whitelist name.
Procedure: Verify the settings fields and reload the whitelist store from disk.
Expected outcome: The whitelist name, categories, and subcategories remain consistent across initialization and reload.
Run: `./build-tests/ai_file_sorter_tests "WhitelistStore initializes from settings and persists defaults"`

#### Test case: CategorizationService builds numbered whitelist context
Purpose: Confirm the whitelist context includes numbered categories and an "any" subcategory fallback.
Setup: Set allowed categories in settings and build a service instance.
Procedure: Call the test access method to build the whitelist context string.
Expected outcome: The context includes numbered category lines and indicates that subcategories are unrestricted.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService builds numbered whitelist context"`

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

#### Test case: CategorizationService parses category output without spaced colon delimiters
Purpose: Ensure category parsing accepts compact `Category:Subcategory` output.
Setup: Use a fixed LLM stub response `Documents:Spreadsheets`.
Procedure: Run `categorize_entries` for one file entry.
Expected outcome: Parsed category is `Documents` and parsed subcategory is `Spreadsheets`.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService parses category output without spaced colon delimiters"`

#### Test case: CategorizationService parses labeled category and subcategory lines
Purpose: Ensure category parsing accepts labeled multiline output.
Setup: Use a fixed LLM stub response with `Category: ...` and `Subcategory: ...` lines.
Procedure: Run `categorize_entries` for one file entry.
Expected outcome: Parsed labels match the provided category and subcategory values.
Run: `./build-tests/ai_file_sorter_tests "CategorizationService parses labeled category and subcategory lines"`

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

### Test infrastructure: `tests/unit/test_cli_reporter.cpp`

This file registers a Catch2 event listener that prints a one-line "[TEST]" banner for each test case as it begins. It does not define test cases itself, but it makes CLI output easier to follow during long runs.
