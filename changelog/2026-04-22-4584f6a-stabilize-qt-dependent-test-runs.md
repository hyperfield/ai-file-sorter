# Stabilize Qt-dependent test runs

## Summary

The runtime test setup was adjusted so Qt-dependent tests can run more reliably in headless and sandboxed environments.

## Motivation

Several tests depended on desktop-session behavior, runtime directories, or model-selection assumptions that were fragile outside a normal interactive Linux desktop. This made unrelated categorization and visual-model test failures obscure the behavior under test.

## Implementation

- Added a test-friendly single-instance runtime directory override so tests do not depend on the host session runtime path.
- Tightened visual-model test setup so tests select the expected visual backend instead of inheriting unrelated defaults.
- Adjusted Qt test scripts and helpers for offscreen execution where needed.
- Added coverage for non-ASCII and emoji category labels so valid Windows folder names remain accepted.

## Validation

Targeted Qt/runtime tests were rerun in the test build, including offscreen execution paths. The change is test infrastructure oriented and does not alter normal production behavior unless the explicit test runtime override is provided.

## User-visible impact

No direct user-facing feature change. The project’s automated test runs are more stable and better cover Unicode category labels.

## Remaining caveats

The runtime override is intended for development and automated tests, not normal app configuration.
