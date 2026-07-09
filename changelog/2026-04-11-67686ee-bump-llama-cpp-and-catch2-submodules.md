# Vendored llama.cpp and Catch2 bump

Commits covered: `67686ee`

## Summary

This commit moved the vendored `llama.cpp` and `Catch2` submodule pointers forward together.

## Motivation

The repository relies on both projects directly:

- `llama.cpp` for local LLM and visual-model runtime behavior
- `Catch2` for the unit test harness

Keeping those submodule pointers synchronized with the branch’s current expectations reduces drift between local development, CI, and future feature work.

## Implementation

There were no first-party source edits in this commit. The entire change was two submodule pointer updates:

- `app/include/external/llama.cpp`
- `external/Catch2`

That makes this a dependency baseline change rather than an app-logic change.

## Validation

Validation is necessarily indirect:

- successful downstream builds
- continued test execution against the updated `Catch2` pin
- local LLM/visual runtime remaining compatible with the updated `llama.cpp` snapshot

No dedicated new tests were added in this commit itself.

## User-visible impact

There is no guaranteed, directly user-visible feature in this commit. The practical effect is that the branch now consumes a different upstream runtime/test baseline, which may bring fixes or compatibility adjustments from those projects.

## Remaining caveats

- Because this is only a pointer update, any upstream behavior change arrives as-is.
- The changelog chapter cannot safely promise a specific user-facing effect without separately auditing the upstream delta.
