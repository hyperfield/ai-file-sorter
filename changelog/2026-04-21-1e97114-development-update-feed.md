# Development Update Feed

## Summary

Commit `1e97114` added a development-mode update feed. When the app starts with `--development`, the updater can read `UPDATE_SPEC_FILE_URL_DEVELOPMENT` instead of the production update feed.

## Motivation

Testing real update behavior against the production feed is risky because the production JSON represents what released users see. Developers need a separate feed that can advertise test versions, test installer URLs, and test metadata without affecting normal users.

## Implementation

`Updater` now accepts a `development_mode` constructor flag. `MainApp::start_updater` passes the app's current development-mode state into the updater.

The updater resolves the feed URL by preferring `UPDATE_SPEC_FILE_URL_DEVELOPMENT` only when development mode is active. If that variable is not set, development mode falls back to `UPDATE_SPEC_FILE_URL`. Normal runs ignore the development feed even if it is present.

The bundled `.env` now includes:

```text
UPDATE_SPEC_FILE_URL_DEVELOPMENT=https://filesorter.app/static/json/aifs_version_dev.json
```

The README documents the primary and development feed variables.

## Validation

`tests/unit/test_updater.cpp` gained tests for three cases:

- development mode uses the development update feed
- development mode falls back to the standard feed when needed
- normal mode ignores the development feed

## User-visible impact

End users should see no change in normal mode. Developers can run the app with `--development` and exercise update behavior against a separate server-side JSON file.

## Remaining caveats

The development feed is only useful if the server-side JSON is kept valid and points to test-safe artifacts. It should not be used as a production fallback.
