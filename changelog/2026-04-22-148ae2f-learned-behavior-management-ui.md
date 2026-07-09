# Add learned behavior management UI

## Summary

The app now exposes learned behavior as a separate user-managed data area instead of treating it like cache data.

## Motivation

Users need a clear way to reset learned categorization behavior without confusing that action with ordinary cache cleanup. Cache clearing should not destroy long-term user-approved category preferences.

## Implementation

- Added a reset action for learned categorization behavior with an explicit warning.
- Kept learned behavior reset separate from categorization cache clearing.
- Reimported current whitelist taxonomy after reset so user-owned category constraints remain available.
- Wired the learning store through the main app, categorization service, and review flow.
- Kept unfinished plugin UI gated to development mode while the public feature remains unavailable.
- Updated UI translations for the new user-facing strings.

## Validation

The app build, targeted learning tests, translation tests, and offscreen test suite were run after the feature wiring.

## User-visible impact

Users can clear temporary cache data without losing learned behavior, and they can explicitly reset learned categorization behavior when they want to start fresh.

## Remaining caveats

Resetting learned behavior removes locally learned examples and aliases. It does not remove user files or normal app settings.
