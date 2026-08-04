# Developer App Test Modes

## Summary

Commit `f510520` adds two developer-oriented runtime test paths to the production executable:

- `--test` opens the normal GUI, implies development mode, and exposes a dedicated **Tests** menu.
- `--self-test` runs deterministic headless checks and exits with a pass/fail status.

The first GUI test preset is a large-whitelist real-LLM categorization run. The headless suite covers the same large-whitelist behavior with a deterministic LLM stub so it can run in CI-like environments.

## Motivation

Large user-maintained whitelists are difficult to validate with ordinary unit tests alone. The app needs two different kinds of checks:

- A deterministic check that proves large whitelists are reduced to compact candidate lists, learned categories can outrank generic model output, and Unicode labels survive prompt selection.
- A real runtime check that launches the app UI and lets a developer run the same kind of scenario through the selected real LLM.

This split avoids using a live local or remote model in automated tests while still giving developers a supported way to test the actual model path manually.

## Implementation

The commit introduces `AppTestRunner` for headless suites and `WhitelistTestFixtures` for shared large-whitelist sample data. The production entry point parses `--self-test` before creating the GUI and dispatches directly to the runner:

```cpp
if (parsed_args.self_test_suite) {
    return run_self_test_mode(parsed_args);
}
```

The GUI path adds test mode as a distinct runtime flag. Test mode gets a separate app-data directory for app-owned databases and fixtures while continuing to use the user's selected LLM settings:

```cpp
const auto profile_dir = Utils::utf8_to_path(settings.get_config_dir()) / "test_mode_profile";
std::filesystem::create_directories(profile_dir);
app_data_dir = Utils::path_to_utf8(profile_dir);
```

`MainApp` now accepts an optional runtime data directory. That directory is used for the categorization database, user-learning store, whitelist store, storage plugin manager, and undo manager. In test mode the app also skips updater startup, avoids save-on-close persistence, and hides the test menu unless `--test` is active.

The GUI preset writes sample files, configures the transient large whitelist, and starts the normal analysis flow so the Review dialog can be inspected with real model output.

## Validation

The commit adds unit coverage for:

- `AppTestRunner` accepting the whitelist suite and rejecting unknown suites.
- The Tests menu being visible only in test mode.
- The test-mode runtime data directory writing `whitelists.ini` into the isolated profile rather than the normal config directory.
- UI translation wiring for the new Tests menu action.

The headless executable path can be checked with:

```sh
./build-tests/aifilesorter --self-test
./build-tests/aifilesorter --self-test=whitelist
```

The GUI path still requires manual validation because it intentionally uses the selected real LLM.

## User-visible impact

Normal users should not see any new UI. Developers launching with `--test` get a Tests menu with a large-whitelist LLM preset. Developers launching with `--self-test` get console pass/fail output instead of the main window.

The test profile is isolated from normal whitelist, cache, learned behavior, and undo data. The LLM configuration is intentionally reused so the GUI preset tests the same backend the user selected.

## Remaining caveats

The GUI test is not a replacement for deterministic automated tests. It depends on the currently selected LLM and must be interpreted manually in the Review dialog.

Because the isolated test profile is persistent, repeated GUI test runs can still be affected by cached sample-folder results unless the preset clears that folder's cache or the developer starts with a fresh test profile. A follow-up should make repeated real-LLM runs explicitly bypass their own sample cache.
