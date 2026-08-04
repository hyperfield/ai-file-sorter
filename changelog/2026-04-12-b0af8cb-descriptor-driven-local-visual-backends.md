# Descriptor-driven local visual backends

Commits covered: `b0af8cb`

## Summary

This commit replaced the hardcoded “single LLaVA download pair” view of image analysis with a descriptor-driven local visual backend system. The app can now describe multiple supported local visual backends, persist the selected backend in settings, resolve backend-specific artifacts at runtime, and adapt prompt shape for different model families.

## Motivation

The previous implementation assumed one baked-in visual stack:

- one text model
- one `mmproj`
- one prompt shape
- one download UI layout

That made it difficult to add or test alternative local visual backends, and it hid backend-specific differences behind LLaVA-specific naming. The app needed a real abstraction if it was going to support more than one local visual model family.

## Implementation

### Backend catalog and runtime resolution

`VisualModelCatalog` now describes each built-in backend in one place:

```cpp
{
    "gemma-3-4b-it",
    "Gemma 3 4B IT",
    VisualModelArchitecture::MtmdProjector,
    VisualPromptPolicy::StructuredVisionInstruct,
    { ... }
}
```

That descriptor records:

- stable backend id
- display name
- architecture family
- prompt policy
- required artifacts and their URL env vars
- fallback filenames for artifact discovery

`VisualLlmRuntime` stopped hardcoding `LLAVA_MODEL_URL` / `LLAVA_MMPROJ_URL` and now resolves a selected backend through those descriptors.

### Analyzer abstraction

The visual path gained:

- `ImageAnalyzer`
- `ImageAnalyzerFactory`
- `ImageAnalysisResult`
- `ImageAnalyzerSettings`

That lets callers depend on a generic analyzer interface while the factory instantiates the correct implementation for the selected backend.

### UI/settings migration

`LLMSelectionDialog` no longer has two special-case LLaVA download rows. It now builds the image-analysis download section from backend descriptors and exposes a backend combo box. `Settings` persists the chosen visual backend id so the dialog and runtime stay aligned between launches.

### Per-backend prompt policy

The commit also introduced prompt-policy branching inside the current MTMD-based analyzer path. Legacy LLaVA backends keep the older prompt wording, while newer instruction-style backends such as Gemma can use a more structured prompt.

## Validation

This change was validated unusually well for a UI/runtime abstraction:

- `tests/unit/test_visual_llm_runtime.cpp` covers catalog contents and backend-specific artifact resolution
- `tests/unit/test_llm_selection_dialog_visual.cpp` checks descriptor-driven backend switching in the UI
- `tests/unit/test_settings_image_options.cpp` verifies the selected backend persists
- `tests/unit/test_llava_image_analyzer.cpp` verifies legacy versus structured prompt-policy behavior
- `README.md` and `TESTS.md` were updated so the documented behavior matches the implementation

## User-visible impact

Users now see “Image analysis models” rather than a LLaVA-only section, and the app can switch among built-in local visual backends without rewriting the runtime path each time.

The built-in catalog at this point includes:

- `llava-v1.6-mistral-7b`
- `llava-v1.6-vicuna-7b`
- `gemma-3-4b-it`

## Remaining caveats

- The embedded local visual runtime still uses the current text-model-plus-`mmproj` MTMD path, so this is not yet an external API multimodal backend system.
- The abstraction is real, but only one architecture family (`MtmdProjector`) exists so far.
