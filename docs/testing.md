# Testing

This page is a short map of the repository's test surfaces. It is meant to
complement, not replace, the detailed case catalog in `TESTS.md`.

## Main test layers

- **Catch2/CTest unit and integration tests**: the normal automated test suite.
- **`TESTS.md`**: detailed case-by-case intent, setup, procedure, and expected
  outcomes.
- **Production self-test mode**: `--self-test` runs all deterministic checks
  from the built app. `--self-test=whitelist` selects the whitelist checks;
  `--self-test=tls` selects the linked platform TLS trust policy on every platform.
- **Opt-in live headless LLM tests**:
  `tests/live_llm/headless_live_llm_tests.py` runs the production headless path
  with a real local model and real/generated fixtures.

## Common commands

Build and run the normal test suite:

```text
cmake --build build-tests --config Release --parallel
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

Windows test targets copy the matching vcpkg runtime DLLs beside each test
executable after the target builds. If a direct `ctest` run fails because a DLL
is missing or stale, rebuild the relevant test target before rerunning `ctest`.
Build all default targets before running the full CTest suite so the storage
conformance executable and Windows updater variants are available too.

Translation tests require generated Qt catalogs. Install the Qt Linguist
development files before configuring, including `qt6-tools-dev` and
`qt6-l10n-tools` on Debian/Ubuntu. CMake rejects configurations without them.
WebP decoding also requires a runtime decoder; Debian/Ubuntu build environments
and packages install `libwebp7` for the fallback when Qt's WebP plugin is absent.

## When to run what

- Prompt or taxonomy changes: run the prompt-builder and categorization-focused
  tests first.
- Headless or Explorer-adjacent changes: run focused `Headless*` tests and keep
  `TESTS.md` in sync.
- Headless process output: with Python 3 available at configure time, building
  `ai_file_sorter_tests` also builds the production executable. Run
  `ctest --test-dir build-tests -C Release -R '^headless_stdout_contract$' --output-on-failure`.
  This checks startup diagnostics, JSON stdout/status files, review-only analysis,
  saved-plan apply, failure output and help using isolated temporary fixtures;
  it does not require a model or external API. Processes launch from a temporary
  working directory. On macOS, loader environment overrides are removed, and
  embedded-PDFium builds also check that a relocated executable loads its adjacent
  runtime from a path containing spaces and non-ASCII characters.
  Windows also runs `headless_launcher_stdout_contract` through the launcher,
  including redirected JSON with `--console-log --development`.
- API image analysis: run `./build-tests/ai_file_sorter_tests "[http-vision]"`.
  Loopback HTTP fixtures cover actual image uploads, API errors, cancellation,
  persisted settings, GUI selection, shared workflow caching and headless renaming;
  no downloaded model or external API is required.
- Live-runner timeout cleanup: run CTest's `live_llm_process_cleanup`. It uses
  real parent/child processes to check output, exit codes and complete process
  termination after a timeout; no model is required.
- Updater/feed changes: run the updater and update-feed tests.
- TLS trust changes: rebuild `aifs_curl_tls_probe` and `ai_file_sorter_tests`, run
  CTest's `curl_tls_trust`, the Catch2 `[tls]` cases, and the production app's
  `--self-test=tls`. On Windows also run `windows_curl_tls_revocation` and
  `windows_tls_layout`; these exercise strict signed-CRL outcomes, a real HTTPS
  CONNECT tunnel, cleared overrides and legacy layout cleanup without importing
  certificates into the machine trust store. See `TESTS.md` for the local certificate fixtures
  and the macOS build/package checks. Use Python 3.10 or newer for integration
  tests; set `Python3_EXECUTABLE` if an existing CMake cache selects an older one.
- Packaging/build-script changes: validate the relevant platform build path in
  addition to unit tests. On native macOS run CTest's `macos_bundle_runtime` (or
  `python3 tests/test_macos_bundle_runtime.py -v`), then build a signed app using
  `app/scripts/create_macos_bundle.sh`. Its runtime audit verifies relocation and
  actual loaded paths in addition to the production TLS self-test.
- Windows Vulkan startup: build the production app, its launcher when present,
  and `aifs_incompatible_vulkan_loader`, then run CTest's `windows_vulkan_startup`.
  Isolated process checks cover loader precedence, CPU fallback before GGML
  imports, direct/Store layouts, and explicit runtime load failures.

## Live headless LLM tests

The opt-in live suite is useful when you want to validate the real runtime,
selected local model, emitted status JSON, and actual filesystem effects. It is
not part of the normal fast unit-test loop.

See these references for the full details:

- `TESTS.md`
- `tests/live_llm/README.md`
