# Vendored llama.cpp and Catch2 Bump

## Summary

Commit `3adae66` advances the vendored `llama.cpp` and `Catch2` submodule pointers.

## Motivation

The project depends on both submodules for core runtime and test behavior:

- `llama.cpp` backs the local text and visual inference paths
- `Catch2` backs the test suite used throughout the repo

Keeping those pointers current is part of maintaining buildability and staying aligned with the runtime assumptions used by recent local-LLM work.

## Implementation

This commit only changes the recorded submodule revisions:

- `app/include/external/llama.cpp`
- `external/Catch2`

There are no first-party source edits in the commit itself. The effect is entirely determined by the new vendored revisions pulled in by those pointers.

## Validation

Validation for a submodule-pointer bump is narrower than for ordinary feature work. The main checks are whether the repo still builds and whether the updated vendored code remains compatible with the surrounding first-party code.

Because this commit is only the pointer change, deeper behavior validation comes from the follow-up feature and test commits built on top of it.

## User-visible impact

There is no direct user-facing UI change tied to this commit alone. Any visible effects depend on the runtime or testing behavior of the vendored revisions and on the first-party commits that consume them.

## Remaining caveats

Submodule bumps always carry some compatibility risk even when no local source files change. If upstream behavior shifts unexpectedly, the symptoms usually appear in later runtime or test changes rather than in the pointer bump itself.
