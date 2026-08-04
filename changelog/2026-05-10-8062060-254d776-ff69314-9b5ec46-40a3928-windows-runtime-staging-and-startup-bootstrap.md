# Summary

This series tightened the Windows startup path for local runtimes. It normalized Vulkan runtime staging during builds, improved how packaged runs locate the right DLL payloads, allowed CPU fallback to reuse the packaged `wvulkan` runtime when that is the only shipped loader, and added a direct-launch bootstrap path for ggml backends before the main window starts.

# Motivation

Windows packaging had several fragile edges around precompiled ggml backends and Vulkan loader placement. Those issues tend to appear only after packaging, especially in MS Store or direct-launch scenarios where the working directory and DLL search path differ from local developer runs.

# Implementation

The build scripts and runtime path helpers were updated together so the same staging assumptions are used at build time and at launch time. The launcher gained logic for reusing the staged Vulkan payload even when the app eventually falls back to CPU execution, which reduces the number of "present but unusable" deployment states.

`main.cpp` also learned how to bootstrap direct-launch ggml backends early enough that the app can start cleanly outside the usual launcher path. The README, `CHANGELOG.md`, and `TROUBLESHOOTING.md` were refreshed in the same batch to document the new Windows packaging expectations for 1.8.0-era builds.

# Validation

This work came with runtime-path test coverage in `tests/unit/test_ggml_runtime_paths.cpp`, plus additional packaging-oriented test catalog updates in `TESTS.md`. The remaining validation was platform-specific manual smoke testing of packaged Windows runs.

# User-visible impact

Packaged Windows builds are less likely to fail at startup because of misplaced or partially staged ggml and Vulkan runtime files. That is especially relevant for users launching the app outside a developer shell or from MS Store style packaging flows.

# Remaining caveats

This was intentionally Windows-specific work. It improves packaging robustness, but it still depends on the shipped runtime payload matching the app build, so bad third-party DLL swaps can still break startup.
