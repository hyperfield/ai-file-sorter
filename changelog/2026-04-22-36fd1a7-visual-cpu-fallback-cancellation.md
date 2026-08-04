# Cancel analysis when visual CPU fallback is declined

## Summary

Declining the visual CPU fallback prompt now cancels the active analysis instead of continuing into categorization.

## Motivation

When image name suggestion analysis cannot use the GPU, the app asks whether to retry on CPU. Pressing Cancel should mean the user wants to stop the process, not skip visual naming and continue with later categorization steps.

## Implementation

- Changed the visual CPU fallback prompt path so a declined retry is treated as analysis cancellation.
- Propagated cancellation through the analysis flow rather than allowing the next phase to continue.
- Added targeted tests for the visual CPU fallback cancellation behavior.
- Removed an obsolete translation source artifact that was no longer needed.

## Validation

The test build was rebuilt and targeted visual CPU fallback tests were run. Translation tests, full offscreen tests, and `git diff --check` were also run after the fix.

## User-visible impact

If users press Cancel in the CPU fallback dialog, the scan stops as expected instead of moving on to categorization.

## Remaining caveats

This specifically covers user-declined CPU fallback. Other non-retryable visual analysis failures continue to follow their existing fallback paths.
