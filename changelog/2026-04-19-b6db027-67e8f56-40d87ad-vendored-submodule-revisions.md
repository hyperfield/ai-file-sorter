# Vendored Submodule Revisions

## Summary

Commits `b6db027`, `67e8f56`, and `40d87ad` adjusted the vendored Catch2 and `llama.cpp` submodule pointers.

## Motivation

The app vendors test and local-LLM dependencies, so submodule pointers need to be kept in a known-good range. The two rewind commits pulled individual dependencies back, while the later bump synchronized both pointers again.

## Implementation

The affected paths were:

- `external/Catch2`
- `app/include/external/llama.cpp`

No app source files changed in these commits. The operational effect comes from the exact dependency revisions checked out by developers and CI.

## Validation

The commits themselves only change pointers. They should be validated by rebuilding the test target and the local LLM integration after submodule initialization.

## User-visible impact

There is no direct UI change. Users may benefit indirectly if the pinned dependency range avoids build failures or runtime regressions.

## Remaining caveats

Submodule pointer changes can affect platform-specific builds. Windows, macOS, and Linux should all be covered before a release if either dependency pointer changes close to release packaging.
