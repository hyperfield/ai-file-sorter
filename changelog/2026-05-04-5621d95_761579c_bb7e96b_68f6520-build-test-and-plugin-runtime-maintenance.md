# Build, Test, and Plugin Runtime Maintenance

## Summary

Commits `5621d95`, `761579c`, `bb7e96b`, and `68f6520` are a maintenance cluster rather than a single feature:

- stage precompiled CPU runtime assets for tests
- isolate visual runtime tests and gate real OneDrive checks
- normalize plugin entry points and plugin process paths
- add a missing Apple Silicon Qt 6 header include

## Motivation

As the app added local visual runtimes, plugins, and broader platform support, the surrounding build/test infrastructure needed small but important corrections to stay reliable:

- tests needed the right runtime pieces staged consistently
- integration-style tests needed firmer isolation boundaries
- plugin execution needed more predictable path normalization
- Apple Silicon builds needed one compatibility header in the right place

## Implementation

The commits make targeted infrastructure corrections rather than changing one user-facing feature. The main themes are:

- make tests less dependent on ambient machine state
- keep plugin runtime invocation paths predictable
- keep platform-specific compilation working on Apple Silicon

## Validation

Validation for this cluster is mostly practical:

- successful local test runs with the staged runtime
- reduced reliance on real external state in gated tests
- plugin path behavior checked through runtime execution paths
- successful Apple Silicon compilation after the header fix

## User-visible impact

The direct user-visible effect is small, but these commits reduce “works on one machine, fails on another” behavior in testing, plugin execution, and Apple Silicon compilation.

## Remaining caveats

This is infrastructure work. It improves reliability around the app, but it is not a substitute for broader end-to-end coverage of every plugin or runtime combination.
