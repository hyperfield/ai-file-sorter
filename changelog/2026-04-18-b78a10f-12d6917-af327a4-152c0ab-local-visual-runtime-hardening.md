# Local Visual Runtime Hardening

## Summary

Commits `b78a10f`, `12d6917`, `af327a4`, and `152c0ab` hardened local visual-model execution. They preserve compatibility with legacy default LLaVA projector paths, add GPU-memory preflight checks, surface analysis failures immediately, and make the Linux wrapper prefer CUDA over Vulkan when both are present.

## Motivation

Local visual analysis is sensitive to hardware, runtime libraries, and model sidecar files. A missing projector, insufficient GPU memory, or wrong backend priority can make the feature fail in ways that are difficult for users to understand. These commits improve the app's ability to choose the expected runtime path and fail early with clearer feedback.

## Implementation

The visual model catalog now reuses the legacy default LLaVA `mmproj` path where appropriate. That avoids breaking existing installations that already downloaded the previous default projector file.

The local LLM client gained GPU-memory preflight behavior before model load. This gives the app an opportunity to detect likely out-of-memory conditions before starting an expensive model initialization path. The related tests expose the preflight logic through `LocalLLMTestAccess`.

The main app now surfaces analysis failures immediately instead of letting coordinator failures remain hidden until later UI refreshes. This affects the user feedback path, not just logging.

The Linux wrapper and Debian packaging script were adjusted so CUDA has priority over Vulkan in wrapper environment setup. That is a Linux-specific runtime preference change.

## Validation

The commits added or updated coverage in:

- `tests/unit/test_llm_selection_dialog_visual.cpp`
- `tests/unit/test_visual_llm_runtime.cpp`
- `tests/unit/test_local_llm_backend.cpp`

The GPU-memory and visual catalog paths are covered by unit tests. The wrapper preference should still be smoke-tested on a Linux system with both CUDA and Vulkan-capable runtime libraries installed.

## User-visible impact

Users with existing visual-model downloads should be less likely to lose compatibility after catalog changes. Users on GPU-constrained systems should get earlier feedback before a model load fails. Linux users with both CUDA and Vulkan available should see CUDA selected first by the launcher wrapper.

## Remaining caveats

GPU-memory preflight is a protective heuristic, not a complete guarantee. Drivers, unified memory behavior, and backend-specific allocation patterns can still make a model fail after passing preflight.
