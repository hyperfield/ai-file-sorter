# Cache Maintenance Actions and Dialog

## Summary

Commit `3940a76` added cache maintenance actions and a dedicated dialog for clearing app-managed data. It introduced service-level cache operations, a UI dialog, translations, tests, and updater/feed support needed to describe cache-related changes.

## Motivation

AI File Sorter stores several kinds of local state: categorization results, image location cache data, and logs. Users need a clear in-app way to inspect and clear those stores without manually deleting files from the config directory. This is especially important when troubleshooting stale categorization results, location lookups, or logs.

## Implementation

The change added `CacheMaintenanceService` for the operational layer and `CacheMaintenanceDialog` for the UI. `DatabaseManager` gained cache-clearing support, while `Logger` gained log-maintenance behavior. The main window exposes the action through the settings flow and wires it into `UiTranslator` for localization.

The update feed and updater test access also gained supporting changes so cache-maintenance-related update text and changelog behavior can be handled consistently by tests.

The commit added translated strings for every supported UI language and expanded documentation in `README.md` and `TESTS.md`.

## Validation

The commit added dedicated tests:

- `tests/unit/test_cache_maintenance_dialog.cpp`
- `tests/unit/test_cache_maintenance_service.cpp`
- `tests/unit/test_main_app_cache_action.cpp`

It also updated tests for the updater, update feed, UI translator, translation coverage, and visual-model runtime paths touched by supporting code.

## User-visible impact

Users can clear cache and log data from the application instead of editing the config directory by hand. This makes troubleshooting safer and makes it clearer which local stores the app maintains.

## Remaining caveats

Cache clearing is intentionally destructive for the selected local store. Users should expect repeated categorization or location lookup work after clearing the relevant cache.
