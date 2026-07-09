# Image Analysis Routing, Diagnostics, and Option Safety

## Summary

Commits `df8cf28`, `ff61449`, `03e0fbb`, `d7a974a`, and `562a42c` tightened the image-analysis path from model selection through prompt construction, runtime diagnostics, and UI option handling. Together, they make local visual categorization more predictable and easier to diagnose when the configured backend or prompt route is wrong.

## Motivation

Image categorization depends on more than a single model file. The app has to pair the right visual model artifacts with the active backend, route screenshots differently from camera photos when needed, and avoid letting incompatible options produce confusing output. Before these changes, a backend mismatch or image-option conflict could produce weak categorization results without making the cause obvious.

## Implementation

The visual model catalog was extended so each backend can resolve the artifacts it actually needs. The downloader and LLM selection dialog now understand backend-specific artifact storage and legacy resolution paths, while tests cover the runtime selection behavior.

Image prompts now include richer file context for screenshot-aware routing. The categorization service can pass image-specific hints into the prompt so local and remote models have a better chance of separating screenshots, photos, scans, and other image-like files.

The local LLM image path was adjusted to prioritize image-first prompts for image file categorization. That reduces reliance on generic text prompts when the user is explicitly analyzing image files.

Runtime diagnostics were added around image analysis. `ImageAnalyzer`, `LlavaImageAnalyzer`, and `AnalysisCoordinator` now expose more concrete failure and backend information so the UI can surface what went wrong instead of leaving users with a vague failed analysis.

The main-window image options were also guarded more carefully. Recursive scan state is preserved, while incompatible image controls are disabled when they should not apply.

## Validation

The grouped commits added and updated focused unit coverage, including:

- `tests/unit/test_llm_selection_dialog_visual.cpp`
- `tests/unit/test_visual_llm_runtime.cpp`
- `tests/unit/test_whitelist_and_prompt.cpp`
- `tests/unit/test_local_llm_backend.cpp`
- `tests/unit/test_main_app_image_options.cpp`

Validation appears to be concentrated in unit tests and prompt/runtime assertions. No broad manual platform smoke test is documented in the commits themselves.

## User-visible impact

Users should see more reliable image categorization, especially when switching between visual backends or categorizing screenshots. If image analysis fails, the app has more context available to report the failure clearly. Image-analysis options should also behave less surprisingly when combined with recursive scans or incompatible modes.

## Remaining caveats

The improvements still depend on the selected model and backend being available locally. Diagnostics can explain more failures, but they do not remove the need for correct model downloads, compatible runtime libraries, and sufficient local hardware.
