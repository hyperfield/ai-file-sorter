# Single-instance startup enforcement

Commits covered: `eda322d`

## Summary

This commit taught the GUI app to behave as a single-instance application. A later launch now notifies the already running primary instance and exits cleanly instead of starting a second independent window.

## Motivation

Launching multiple copies of a file-sorting app is risky:

- users can end up with duplicate long-running scans
- prompts and review dialogs become ambiguous
- updater and storage-plugin flows can race each other

The right behavior is the usual desktop-app model: one primary process owns the UI, and later launches reactivate it.

## Implementation

The new `SingleInstanceCoordinator` combines a lock file with a local IPC endpoint:

```cpp
if (!instance_guard.acquire_primary_instance()) {
    return EXIT_SUCCESS;
}
```

The primary instance:

- acquires a `QLockFile`
- listens on a `QLocalServer`

A secondary instance:

- fails to take the lock
- connects to the primary via `QLocalSocket`
- sends an activation request
- exits without starting the rest of the app

`main.cpp` wires the activation callback to the best available window target, restoring and focusing it when possible.

The same commit also made the build system ready for this feature:

- `Qt6::Network` was added in CMake because `QLocalServer` / `QLocalSocket` live there
- SQLite target resolution in CMake was hardened so tests and builds remain portable across different package layouts

## Validation

Validation is explicit and automated:

- `tests/unit/test_single_instance_coordinator.cpp` was added
- one test proves a relaunch notifies the primary instance
- another test proves different logical instance ids can coexist

## User-visible impact

Users who relaunch the app now get the expected desktop behavior:

- the existing instance is activated
- no second independent sorter window is started

## Remaining caveats

- If the notification channel fails, the lock still prevents a full second launch, but the primary window might not be activated.
- The behavior depends on per-user runtime/temp paths for the local server and lock file.
