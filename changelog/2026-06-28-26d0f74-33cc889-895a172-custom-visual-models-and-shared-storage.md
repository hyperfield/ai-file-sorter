# Summary

This chapter added first-class support for custom visual local models and configurable model storage, while also tightening the built-in visual catalog so Gemma 4B downloads are not duplicated unnecessarily. A dependency-pointer update landed in the same batch to keep the runtime aligned with the new visual-model flow.

# Motivation

The project already supported custom text GGUF models, but visual analysis still leaned too heavily on fixed built-in descriptors and default storage paths. Users needed a way to point the app at both halves of a custom visual stack, especially the text model and its matching `mmproj`, and they needed more control over where those large model files live on disk.

# Implementation

`CustomLLM` entries gained an `mmproj_path`, the selection dialog gained a model-storage override and a visual-backend refresh path for custom entries, and `VisualLlmRuntime` learned how to resolve custom backend ids into validated model/mmproj pairs. At the same time, the built-in visual catalog stopped redownloading Gemma 4B artifacts that can safely reuse the shared storage path.

```cpp
inline bool is_visual_custom_llm(const CustomLLM& entry) {
    return is_valid_custom_llm(entry) && !entry.mmproj_path.empty();
}
```

This small predicate captures the behavioral shift: a custom local model is no longer just "some GGUF file" when the user wants image analysis. The app now explicitly distinguishes text-only custom entries from visual-capable ones.

# Validation

Validation came from:

- `tests/unit/test_custom_llm.cpp`
- `tests/unit/test_visual_llm_runtime.cpp`
- `tests/unit/test_utils.cpp`
- updated README and `TESTS.md` coverage notes

# User-visible impact

Users can register custom visual LLMs with both the text-model GGUF and the matching `mmproj`, pick a non-default local model storage directory, and avoid duplicate Gemma 4B visual downloads when the shared artifact is already present.

# Remaining caveats

Custom visual models still depend on the user supplying a valid GGUF plus matching `mmproj`. The app validates those files more clearly now, but it cannot infer compatibility if the pair itself is wrong.
