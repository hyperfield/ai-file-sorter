# GPU Offload Retry Ladders for Text and Vision

## Summary

Commits `a18f63a` and `8bbf5b1` form one local-runtime hardening sequence:

- `a18f63a` adds a visual-model headroom cap and batch backoff for image analysis
- `8bbf5b1` extends the same idea into an explicit `n_gpu_layers` retry ladder for both text and visual model loading

Together, they change GPU startup from a one-shot guess into a probe-and-backoff flow that tries to preserve GPU acceleration without immediately dropping to CPU after one failed load.

## Motivation

The app had two failure modes on constrained CUDA and Vulkan systems:

1. visual image analysis could estimate too many layers or too large a batch for the available headroom left after loading the visual text model and `mmproj`
2. text categorization could pick an optimistic offload count, fail model load once, and then abandon GPU completely

This was especially visible on 4 GB-class GPUs where one estimate could be only slightly too high. In those cases, a lower `n_gpu_layers` value often would have worked, but the runtime never tried it.

## Implementation

### Visual headroom cap

The visual path now reduces the candidate GPU-layer count before model load when a guarded backend would otherwise crowd out `mmproj` or multimodal evaluation buffers:

```cpp
const int32_t capped_layers = estimate_visual_n_gpu_layers_with_headroom(
    model_path,
    mmproj_path,
    *backend_name,
    memory->free_bytes,
    memory->total_bytes);
if (capped_layers < 0 || capped_layers >= params.n_gpu_layers) {
    return params;
}
params.n_gpu_layers = capped_layers;
```

This is the first line of defense for visual startup. It keeps the visual text model from consuming so much GPU memory that the projector or multimodal decode path immediately becomes unstable.

### Shared retry ladder

The follow-up commit adds a descending retry ladder for strict `n_gpu_layers` attempts:

```cpp
std::vector<int> build_gpu_layer_retry_candidates(int optimistic_layers,
                                                  int conservative_layers)
{
    std::vector<int> candidates;
    append_unique_positive_retry_layers(candidates, optimistic_layers);
    append_unique_positive_retry_layers(candidates, conservative_layers);

    int current_layers = candidates.empty() ? 0 : candidates.back();
    while (current_layers > kMinimumGpuLayerRetryCount) {
        current_layers = reduced_retry_layers(current_layers);
        append_unique_positive_retry_layers(candidates, current_layers);
    }

    return candidates;
}
```

For example, an optimistic `20` with a conservative `15` now yields:

`20 -> 15 -> 11 -> 8 -> 6 -> 4 -> 3 -> 2 -> 1`

That ladder is now used instead of failing once and switching directly to CPU.

### Visual-specific backoff beyond layers

The visual runtime still has additional safeguards that the text path does not need:

- `mmproj` can be moved to CPU when GPU headroom is too tight
- context creation retries use smaller `n_batch` values
- image-evaluation retries can reduce multimodal batch sizes

That combination matters because visual startup pressure comes from both the language model and the multimodal support path.

## Validation

The sequence was validated with focused build and test checks:

```bash
cmake --build build-tests --target ai_file_sorter_tests -j4
cmake --build build-tests --target aifilesorter -j4
./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer*"
./build-tests/ai_file_sorter_tests "LocalLLMClient*"
```

Targeted regression coverage now locks in:

- the exact visual retry ladder
- the exact text retry ladder
- deduplication when optimistic and conservative estimates match
- continued visual handling for tight `mmproj` headroom

## User-visible impact

Users on marginal GPUs should see fewer cases where local AI falls back to CPU after one failed GPU start. The app now has a better chance of finding a slightly smaller offload count that still works.

On the visual side, image analysis can stay slower-but-functional by reducing layer count or batch size instead of failing early. On the text side, categorization can keep partial GPU acceleration instead of dropping straight to CPU.

## Remaining caveats

The runtime is still heuristic-driven before it starts probing. The new behavior is more resilient because it retries lower values, but it still depends on current free memory, backend reporting, and allocator behavior at runtime.

The retry ladder also improves startup robustness, not peak throughput. If the runtime lands on a lower surviving `n_gpu_layers` value, inference will be more stable but potentially slower than the original optimistic target.
