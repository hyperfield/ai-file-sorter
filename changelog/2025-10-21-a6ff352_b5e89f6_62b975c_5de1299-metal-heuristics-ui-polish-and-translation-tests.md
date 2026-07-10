# Metal Heuristics, UI Polish, and Translation Tests

## Summary

Commits `a6ff352`, `b5e89f6`, `62b975c`, and `5de1299` combine three related directions:

- add a heuristic GPU-parameter algorithm for Metal-backed local inference
- improve the main window interface
- fix a main-window memory leak
- introduce translation test automation

## Motivation

The app was becoming simultaneously more GPU-aware and more translation-aware. That meant backend heuristics, UI structure, and regression checks all needed attention at the same time.

## Implementation

The sequence included:

- a sizable Metal-specific heuristic path in `LocalLLMClient.cpp`
- main-window UI refinements in `MainApp.cpp`
- ownership/lifetime cleanup to address a leak in the main window
- a new `tests/run_translation_tests.sh` path to keep Qt translation assets under test

## Validation

Validation included:

- local Metal inference smoke checks
- manual UI verification of the main window changes
- translation test execution via the new translation test script

## User-visible impact

macOS users gained a more deliberate Metal GPU setup path, while all users benefited from a cleaner main window and the start of repeatable translation testing.

## Remaining caveats

The Metal heuristic was still heuristic rather than probe-based, and translation coverage tests only measure completeness, not translation quality.
