# Testing

This page is a short map of the repository's test surfaces. It is meant to
complement, not replace, the detailed case catalog in `TESTS.md`.

## Main test layers

- **Catch2/CTest unit and integration tests**: the normal automated test suite.
- **`TESTS.md`**: detailed case-by-case intent, setup, procedure, and expected
  outcomes.
- **Production self-test mode**: `--self-test` and `--self-test=whitelist`
  provide deterministic checks from the built app.
- **Opt-in live headless LLM tests**:
  `tests/live_llm/headless_live_llm_tests.py` runs the production headless path
  with a real local model and real/generated fixtures.

## Common commands

Build and run the normal test suite:

```text
cmake --build build-tests --config Release --target ai_file_sorter_tests --parallel
ctest --test-dir build-tests -C Release --output-on-failure
```

Run a single Catch2 case:

```text
./build-tests/ai_file_sorter_tests "<test case name or pattern>"
```

On Windows multi-config builds, the direct executable lives under
`./build-tests/tests/<Config>/`, for example:

```text
./build-tests/tests/Release/ai_file_sorter_tests.exe "<test case name or pattern>"
```

## When to run what

- Prompt or taxonomy changes: run the prompt-builder and categorization-focused
  tests first.
- Headless or Explorer-adjacent changes: run focused `Headless*` tests and keep
  `TESTS.md` in sync.
- Updater/feed changes: run the updater and update-feed tests.
- Packaging/build-script changes: validate the relevant platform build path in
  addition to unit tests.

## Live headless LLM tests

The opt-in live suite is useful when you want to validate the real runtime,
selected local model, emitted status JSON, and actual filesystem effects. It is
not part of the normal fast unit-test loop.

See these references for the full details:

- `TESTS.md`
- `tests/live_llm/README.md`
- [Windows release builds](windows-release-builds.md)
