# Local LLM Build and Dialog Cleanup

## Summary

Commits `9acc727`, `975a391`, `0eb264b`, `12cfac5`, and `99bd0ed` cleaned up the local-LLM and startup environment around late October 2025:

- improve the Linux `llama.cpp` build script for CUDA reliability
- refine CUDA `n_gpu_layers` selection
- remove stale OpenCL mentions from the LLM dialog
- remove obsolete X11 guards from `main.cpp`
- slightly retune splash timing

## Motivation

This was a cleanup sequence around the early local-LLM rollout: the build scripts, heuristics, and UI wording all still reflected older assumptions.

## Implementation

The work touched:

- Linux build scripting for the vendored `llama.cpp` path
- `LocalLLMClient.cpp` and `Utils.*` for CUDA layer selection behavior
- `LLMSelectionDialog.cpp` for updated wording now that OpenCL was no longer the message the UI should lead with
- bootstrap code in `main.cpp`

## Validation

Validation was a mix of:

- Linux build-script smoke checks with CUDA
- local CUDA heuristic verification
- manual dialog review for the wording cleanup

## User-visible impact

Users saw a more current local-LLM dialog, slightly cleaner startup behavior, and better odds that the Linux/CUDA local-LLM path would build and choose usable GPU settings.

## Remaining caveats

This sequence tuned the early heuristic/build path, but later commits were still needed to harden GPU probing and fallback behavior more substantially.
