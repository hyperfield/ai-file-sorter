# Windows CUDA Startup and Backend State Hardening

## Summary

Commits `8816d58`, `c0a1a72`, `7dfbcc6`, and `39d6f8b` form a short runtime-hardening sequence for local GPU execution, with most of the user-visible effect landing on Windows:

- Windows build scripts and packaging were tightened so the right CUDA runtime pieces are found and staged
- CUDA runtime DLL detection started preferring the most relevant toolkit/runtime candidates instead of whichever DLL appeared first
- visual image-analysis GPU startup gained a Windows preflight subprocess so bad runtime setups fail early and cleanly
- local LLM execution restores temporary backend environment changes and frees visual state more reliably before downstream text stages

## Motivation

The local AI stack had become sensitive to two kinds of runtime fragility:

1. Windows CUDA startup could succeed or fail based on runtime DLL ordering, packaging layout, or missing toolchain/runtime pieces
2. the app’s own environment changes could leak between visual and text phases, leaving later stages to inherit a stale backend state

The net effect was avoidable GPU startup failures, especially on Windows, and harder-to-diagnose follow-on failures after visual analysis had already run.

## Implementation

### Smarter Windows CUDA runtime ranking

The CUDA probe now ranks runtime candidates by source, path kind, and parsed toolkit/runtime version instead of treating all discovered DLLs equally:

```cpp
int runtime_directory_source_priority(RuntimeDirectorySource source)
{
    switch (source) {
    case RuntimeDirectorySource::ToolkitHint:
        return 0;
    case RuntimeDirectorySource::Path:
        return 1;
    }
    return 2;
}
```

This matters because Windows systems often expose several `cudart64_*.dll` candidates from toolkit installs, PATH entries, and PhysX leftovers. The probe now prefers the most plausible CUDA toolkit runtime instead of an arbitrary earlier hit.

### Visual GPU preflight on Windows

Image analysis now runs a subprocess preflight on Windows before attempting real visual GPU startup:

```cpp
#ifdef _WIN32
if (settings.use_gpu && !should_skip_visual_gpu_preflight()) {
    run_visual_gpu_preflight(backend);
}
#endif
```

That check catches startup failures in a contained subprocess before the main analysis path commits to the visual GPU backend. If the probe crashes or times out, the app can surface a cleaner failure path instead of taking the whole analysis flow down with it.

### Backend environment restoration

Local backend overrides are now wrapped in a scoped restore helper:

```cpp
class ScopedBackendEnvRestore {
public:
    ScopedBackendEnvRestore()
        : snapshot_(capture_backend_env()) {}

    ~ScopedBackendEnvRestore() {
        if (active_) {
            restore_backend_env(snapshot_);
        }
    }
```

This matters because both visual and text local runtimes sometimes force backend-related environment variables such as `AI_FILE_SORTER_GPU_BACKEND`, `LLAMA_ARG_DEVICE`, and `GGML_DISABLE_CUDA`. Without restoration, a temporary CPU or Vulkan override could unintentionally bleed into later model loads.

## Validation

This sequence was validated through a mix of build and focused runtime tests:

- `tests/unit/test_windows_cuda_probe.cpp`
- `tests/unit/test_main_app_visual_fallback.cpp`
- local LLM and visual-runtime smoke paths that exercised the backend env restoration behavior

The Windows-specific pieces were primarily validated through build/runtime logic and targeted tests rather than broad cross-platform behavior claims.

## User-visible impact

On Windows, local GPU startup should be more predictable:

- the correct CUDA runtime is more likely to be selected
- obviously bad visual GPU startup paths fail earlier and more cleanly
- follow-on text stages are less likely to inherit a broken backend environment after visual analysis

Outside Windows, the main visible benefit from this cluster is the safer backend environment restoration between local AI stages.

## Remaining caveats

The preflight adds some startup work on Windows when visual GPU analysis is enabled. That is a tradeoff in favor of cleaner failure handling.

DLL discovery and runtime ranking are still heuristic. They are much more deliberate now, but Windows installations with unusual CUDA layouts can still expose edge cases.
