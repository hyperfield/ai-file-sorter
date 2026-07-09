# Sync vendored dependency pins

Commits covered: `7435f11`

## Summary

This commit adjusted the vendored `llama.cpp` and `Catch2` submodule pointers again so the branch used the dependency pair expected by the current worktree.

## Motivation

When a branch is moving quickly in both runtime code and tests, submodule pointers can drift out of alignment with the code around them. This commit was a pin-sync step: no first-party logic changed, but the branch’s vendored runtime and test framework baseline did.

## Implementation

Only the two submodule pointers changed:

- `app/include/external/llama.cpp`
- `external/Catch2`

No application source files were edited in the same commit.

## Validation

As with the earlier dependency-pin commit, validation is indirect:

- successful compilation against the synchronized vendored tree
- continued test execution against the synchronized `Catch2` baseline

## User-visible impact

There is no guaranteed direct feature change. The effect is repository consistency: the app and tests are now anchored to a different upstream dependency snapshot.

## Remaining caveats

- Submodule sync commits inherit upstream behavior changes without describing them exhaustively.
- Anyone auditing a user-visible regression still needs to inspect the upstream `llama.cpp` / `Catch2` delta separately.
