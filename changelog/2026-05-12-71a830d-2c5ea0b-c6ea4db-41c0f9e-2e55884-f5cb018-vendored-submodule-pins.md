# Summary

These commits moved the vendored `llama.cpp` and Catch2 submodule pointers several times while the surrounding packaging and runtime work was landing. The end result was not a user-facing feature on its own; it was version alignment for the dependencies that the app was already depending on.

# Motivation

The surrounding May build and packaging work was changing Linux packaging, macOS release tooling, Windows runtime staging, and local-model behavior at the same time. Keeping the vendored submodules pinned to known-good revisions was necessary so those changes could be built and tested against a reproducible dependency baseline.

# Implementation

No first-party app logic changed in these commits. The repository pointers for:

- `app/include/external/llama.cpp`
- `external/Catch2`

were advanced or repointed so the build scripts and tests could target specific upstream revisions instead of whatever happened to be current in the submodules.

# Validation

Validation here was indirect: successful builds and tests for the feature work that depended on these pins. There was no separate runtime behavior to validate beyond making sure the surrounding changes built cleanly with the selected revisions.

# User-visible impact

There is no direct UI change from this chapter. The value is in more predictable builds, test runs, and release packaging while fast-moving upstream dependencies were being integrated.

# Remaining caveats

Submodule pointer changes age quickly. This chapter records why the pins moved at the time, but it does not imply that those exact revisions remain ideal for later releases.
