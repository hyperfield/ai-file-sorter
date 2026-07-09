# Summary

This batch added a real macOS release pipeline instead of ad hoc bundle assembly, and it made local Metal model loading more resilient. The image-analyzer preflight helpers were also scoped back to Windows where they belonged, reducing cross-platform noise.

# Motivation

macOS packaging had reached the point where manual bundling was too easy to get wrong, especially once multiple runtime variants and signing-sensitive binaries were involved. At the same time, local Metal model initialization needed a better retry path instead of failing immediately on the first optimistic load attempt.

# Implementation

The release tooling gained explicit scripts to build bundles, create DMGs, and verify release outputs. In parallel, the local LLM client gained Metal retry candidates so a failed optimistic GPU-layer count could step down instead of hard failing. The image-analyzer preflight helpers were narrowed to Windows-only code paths so they would not shape non-Windows behavior accidentally.

The net effect is that macOS release production became scriptable and inspectable, while Metal-backed local inference became less brittle during startup.

# Validation

Validation was mainly script and build oriented:

- new macOS bundle and DMG helper scripts
- `verify_macos_release.sh`
- local-Metal startup smoke checks

The preflight scoping change was a low-risk cleanup, but it mattered for keeping platform-specific code honest.

# User-visible impact

macOS releases became easier to build reproducibly, and Apple users with local Metal inference were less likely to hit a hard startup failure from an aggressive initial GPU configuration.

# Remaining caveats

This chapter improved the release path and the retry ladder, but it did not remove the underlying variability of Apple GPU memory pressure. Real-world Metal availability still depends on the machine, model size, and competing workload.
