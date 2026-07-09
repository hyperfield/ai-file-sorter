# Summary

This small Windows-specific fix decoupled the starter executable from ggml backend headers and backend-loading code. The launcher can now compile against `GgmlRuntimePaths` without pulling in runtime backend-loading dependencies that only the main app actually needs.

# Motivation

The Windows starter is supposed to locate and hand off to the main executable, not behave like the full app. When the shared runtime-path helper pulled in ggml backend-loading headers unconditionally, the starter target inherited build-time coupling it did not need.

# Implementation

The starter target now defines `AI_FILE_SORTER_GGML_RUNTIME_PATHS_NO_BACKEND_LOADING`, and `GgmlRuntimePaths.cpp` uses that to compile out direct backend-loading code and its `ggml-backend.h` dependency for the launcher build.

```cpp
#ifdef AI_FILE_SORTER_GGML_RUNTIME_PATHS_NO_BACKEND_LOADING
    (void)logger;
#else
    ...
    ggml_backend_load_all();
#endif
```

That compile-time split keeps one runtime-path implementation while avoiding unnecessary launcher coupling.

# Validation

Validation here was build-oriented: the Windows starter target could compile cleanly without ggml backend headers being part of its dependency surface.

# User-visible impact

There is no new visible feature, but the Windows launcher build is less fragile and easier to keep independent from ggml backend internals.

# Remaining caveats

This only changes the launcher target. The main app still depends on the normal backend-loading path where appropriate.
