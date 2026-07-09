# Translation Drift Check

## Summary

Commit `2e9fa14` adds a dedicated translation drift check and sync pass so the committed Qt catalogs are verified against the current source tree instead of being updated only opportunistically.

## Motivation

The UI had already grown enough that it was easy for translatable strings to drift out of sync with the committed `.ts` catalogs. A plain catalog refresh commit helps once, but it does not protect future changes unless the project also has a repeatable check.

## Implementation

The commit paired a catalog sync with a source-driven verification step that runs `lupdate`/`lrelease` against temporary copies of the translation catalogs and fails if new `unfinished` entries appear.

That turned translation maintenance from an occasional manual task into something the repo can check deliberately when UI text changes.

## Validation

The validation for this change is the check itself: it exercises the Qt translation toolchain against the active source tree and the committed catalogs.

## User-visible impact

There is no direct feature toggle in the UI, but it reduces the chance of shipping new English-only strings or stale translations after interface changes.

## Remaining caveats

This commit checks translation coverage, not translation quality. A string can be present in every catalog and still need wording improvement.
