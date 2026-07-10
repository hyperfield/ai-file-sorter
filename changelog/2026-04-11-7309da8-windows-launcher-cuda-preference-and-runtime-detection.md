# Windows launcher runtime detection and CUDA-first selection

Commits covered: `7309da8`

## Summary

This was a substantial Windows runtime change. The launcher was restored as the non-MSIX entry point, backend auto-selection changed from `Vulkan -> CUDA -> CPU` to `CUDA -> Vulkan -> CPU`, and Windows gained a much more deliberate CUDA runtime probe instead of a shallow DLL-presence check.

## Motivation

The older Windows launcher flow had two weaknesses:

- runtime detection was too shallow for packaged CUDA payloads
- backend priority favored Vulkan even on systems where packaged CUDA support was present and intentional

At the same time, the packaging story had drifted: the repository needed a clearer distinction between standard/standalone Windows builds that bootstrap into the real app binary and the MSIX path that runs directly.

## Implementation

### Package kind became explicit

`app/CMakeLists.txt` gained `AI_FILE_SORTER_WINDOWS_PACKAGE_KIND` with `STANDARD`, `MSIX`, and `STANDALONE`. For non-MSIX builds, the visible launcher remains `aifilesorter.exe` and the Qt app binary becomes `aifilesorter-bin.exe`.

### CUDA probing became real

The new `WindowsCudaProbe` helper does more than look for one DLL. It checks:

- whether the NVIDIA driver is present
- whether a usable CUDA runtime exists
- which toolkit/runtime version is best
- whether the packaged `ggml-cuda.dll` backend can actually be loaded with that runtime

That is what turns the launcher decision into a compatibility check instead of a guess.

```cpp
availability.cudaAvailable =
    availability.hasNvidiaDriver &&
    availability.cudaRuntimeDetected &&
    availability.cudaPayloadPresent;
```

### Backend priority flipped to CUDA-first

The launcher now resolves auto mode in this order:

```cpp
if (availability.cudaAvailable) {
    return BackendSelection::Cuda;
}
if (availability.vulkanAvailable) {
    return BackendSelection::Vulkan;
}
```

That same priority change was reflected in README and in local backend tests.

### Build tooling now resolves CUDA more robustly

`build_llama_windows.ps1` stopped relying on one hard-coded toolkit path and instead searches:

- `CUDA_PATH`
- `CUDA_PATH_V*`
- standard toolkit install directories under `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA`

It also sets `CUDAToolkit_ROOT` and the CUDA toolchain explicitly for CMake when building CUDA variants.

## Validation

Validation here was stronger than a normal launcher change:

- new tests in `tests/unit/test_local_llm_backend.cpp` cover CUDA-first auto-selection and Vulkan fallback when CUDA is disabled
- README and `TESTS.md` were updated to describe the new priority and launcher behavior
- the build script and launcher logic were both updated in the same commit, reducing “docs say one thing, script does another” drift

## User-visible impact

On Windows standard and standalone builds:

- `aifilesorter.exe` again acts as the visible launcher
- packaged CUDA is preferred over Vulkan when both are available
- CUDA runtime mismatch reporting is more precise
- the launcher is less likely to choose a backend that exists on paper but is unusable in the packaged app

## Remaining caveats

- The logic is Windows-specific and depends on how the packaged `ggml` payloads are staged.
- Runtime detection is still heuristic in the sense that it scans known toolkit locations and packaged payload directories; it is not querying an official package registry.
