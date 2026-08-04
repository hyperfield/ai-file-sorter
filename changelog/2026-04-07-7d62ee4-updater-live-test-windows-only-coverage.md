# Windows-only coverage for updater live-test synthesis

Commits covered: `7d62ee4`

## Summary

This commit narrowed one updater test so it only runs on Windows, and it updated the expected synthetic version string to use the numeric-only version form.

## Motivation

The updater’s live-test synthesis path is Windows-specific in practice because it assumes Windows installer semantics and a Windows download shape. After prerelease support landed, the test also needed to stop appending `.1` to the display string `1.8.0 beta`.

Running that scenario unconditionally on non-Windows hosts made the suite less truthful and more fragile.

## Implementation

Two changes were made:

- the test case was wrapped in `#ifdef _WIN32`
- the expected synthetic version switched from `APP_VERSION.to_string() + ".1"` to `APP_VERSION.to_numeric_string() + ".1"`

That keeps the test aligned with the updated `Version` behavior while making the platform scope explicit.

## Validation

Validation is the test change itself. The Windows build continues to exercise the updater live-test synthesis path, while non-Windows platforms stop pretending to cover a Windows-specific behavior.

## User-visible impact

There is no direct product-facing change. The value of this commit is test accuracy: the suite now reflects that the synthetic updater path is Windows-oriented.

## Remaining caveats

- Non-Windows platforms now have less direct coverage of that exact path, but that is more honest than keeping a misleading cross-platform test.
