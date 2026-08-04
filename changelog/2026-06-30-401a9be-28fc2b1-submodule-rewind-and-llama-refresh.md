# Summary

These two commits adjusted vendored dependency pointers at the end of June: first by rewinding both `llama.cpp` and Catch2, then by moving the `llama.cpp` pointer forward again. Together they record a short dependency-reset cycle rather than a new application feature.

# Motivation

Dependency-pointer resets like this usually happen when a freshly advanced upstream revision turns out to be a poor fit for the current app branch, or when the project wants to temporarily return to a known-good baseline before re-advancing a single dependency more selectively.

# Implementation

The first commit rewound both:

- `app/include/external/llama.cpp`
- `external/Catch2`

The second commit then advanced only the vendored `llama.cpp` pointer again. No first-party app code changed in this chapter.

# Validation

Validation was indirect and branch-oriented: the relevant question was whether the app and test suite behaved correctly against the selected vendored revisions after the pointer adjustments.

# User-visible impact

There is no direct UI change here. The practical effect is a better-aligned dependency baseline for the surrounding late-June work.

# Remaining caveats

Submodule pointer chapters are snapshots of repository state, not long-term guarantees. Later commits may intentionally move these dependencies again for new runtime, build, or test reasons.
