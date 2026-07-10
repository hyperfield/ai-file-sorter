# Summary

This late-June batch mixed runtime polish, test infrastructure, packaging cleanup, and screenshot maintenance. The two biggest behavior changes were headless Windows UI regression coverage and deferred local-backend probing so ggml runtime checks happen after the main window is shown instead of too early during startup. The same batch also aligned macOS bundle and DMG naming, refreshed vendored `llama.cpp` pointers, and updated screenshots.

# Motivation

Several problems here were about timing and release hygiene rather than core categorization:

- Windows UI regressions were hard to cover in CI without a headless test path
- eager backend probing could make startup noisier or more fragile than necessary
- macOS variant naming had drifted between scripts and generated outputs
- screenshots and submodule pointers needed cleanup around the same release window

# Implementation

The CMake and test harness changes made headless Windows UI regression coverage practical for the project test suite. Separately, backend probing moved later in the startup sequence so window construction finishes before ggml availability checks run. The macOS release scripts were adjusted so bundle names and DMG names stay in sync, and the screenshot commits cleaned out obsolete macOS captures while adding a corrected before/after asset.

# Validation

Validation came from:

- the new headless Windows UI test coverage
- updated `TESTS.md` entries
- macOS packaging script smoke checks
- screenshot and submodule review as part of release prep

# User-visible impact

Startup behavior is calmer for local-backend users, Windows UI regressions are easier to catch before release, and macOS packaging outputs are less confusing because the generated names are consistent across scripts and artifacts.

# Remaining caveats

This chapter mixed code, test, documentation, screenshot, and dependency work. The startup timing fix is the primary runtime change; the rest mostly improves release hygiene and confidence around it.
