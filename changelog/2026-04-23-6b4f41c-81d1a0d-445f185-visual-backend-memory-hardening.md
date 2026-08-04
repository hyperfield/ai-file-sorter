# Visual Backend Memory Hardening

## Summary

Commits `6b4f41c`, `81d1a0d`, and `445f185` form a short visual-runtime stability sequence:

- `6b4f41c` guards the visual projector (`mmproj`) against CUDA and Vulkan GPU memory pressure.
- `81d1a0d` adds runtime hint plumbing and applies conservative caps to the Vicuna visual backend.
- `445f185` removes the Vicuna backend and its runtime hints after the safer path became to keep only the supported Gemma and LLaVA Mistral descriptors.

The final state keeps the CUDA/Vulkan projector memory guard and removes the Vicuna backend from the selectable visual model catalog.

## Motivation

Image analysis can fail hard when the text model fits on GPU but the multimodal projector needs more memory than remains available. That is especially risky on CUDA and Vulkan because the app can reach the image-analysis stage after the text model has already consumed most of the device memory.

The goal was to avoid GPU out-of-memory failures by choosing CPU for the visual projector when there is not enough headroom. A separate attempt to make Vicuna safer through runtime caps was superseded by removing that backend from the catalog.

## Implementation

The memory guard estimates how much GPU memory the projector needs by inflating the projector file size and adding a fixed 512 MiB headroom requirement:

```cpp
size_t required_mmproj_gpu_bytes(std::uintmax_t file_size) {
    constexpr size_t kMinHeadroomBytes = 512ULL * 1024ULL * 1024ULL;
    const size_t mmproj_bytes = static_cast<size_t>(
        std::min<std::uintmax_t>(file_size, std::numeric_limits<size_t>::max()));
    const size_t inflated_bytes = mmproj_bytes + (mmproj_bytes / 3);
    return inflated_bytes + kMinHeadroomBytes;
}
```

CUDA and Vulkan are treated as guarded backends. If memory metrics are missing or free memory is below the projected requirement, the app keeps the projector on CPU instead of risking a GPU allocation failure:

```cpp
bool is_mmproj_memory_guarded_backend(std::string_view backend_name) {
    return case_insensitive_contains(backend_name, "cuda") ||
           case_insensitive_contains(backend_name, "vulkan");
}
```

`81d1a0d` briefly added `VisualModelRuntimeHints` with `image_max_tokens` and `max_batch_size` for Vicuna. `445f185` then removed the Vicuna descriptor and the hint plumbing, leaving the catalog with the supported Gemma 3 4B IT default and LLaVA 1.6 Mistral backend:

```cpp
CHECK(find_visual_model_descriptor("llava-v1.6-vicuna-7b") == nullptr);
```

## Validation

The guard is covered by `LlavaImageAnalyzer keeps guarded visual projectors on CPU when headroom is tight`. The test checks that CUDA and Vulkan keep the projector off GPU under tight memory, that CUDA can use GPU when memory is comfortable, and that Metal is not affected by this guard.

The catalog behavior is covered by `Default visual model descriptor exposes the MTMD backend catalog`, which now verifies that the Vicuna descriptor is absent while Gemma and LLaVA Mistral remain available.

## User-visible impact

Users with constrained CUDA or Vulkan GPUs should see fewer image-analysis crashes caused by visual-projector memory pressure. In low-headroom cases, image analysis may run with the projector on CPU, which is slower but safer.

Users should no longer see the Vicuna visual backend as a selectable or resolvable local visual model option. Existing default visual behavior remains centered on Gemma 3 4B IT, with LLaVA Mistral still available as the legacy LLaVA backend.

## Remaining caveats

The memory guard is heuristic. It uses projector file size plus fixed headroom rather than exact runtime allocator requirements, so it favors avoiding crashes over maximizing GPU usage.

CUDA memory probing has a direct query path, but other backend memory metrics still depend on available `ggml` backend reporting. If metrics cannot be read for guarded backends, the projector falls back to CPU.
