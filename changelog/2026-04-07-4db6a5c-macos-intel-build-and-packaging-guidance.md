# macOS Intel build and packaging guidance

Commits covered: `4db6a5c`

## Summary

This documentation commit clarified the macOS Intel build flow in `README.md`, including architecture-specific llama runtime commands and a direct `make` target for native Intel Macs. It also stated plainly that the documented build steps produce raw executables, not a finished `.app` bundle or `.dmg`.

## Motivation

The earlier macOS instructions already covered Homebrew, Qt, and the embedded llama runtime, but they left too much room for ambiguity around:

- how to build Apple Silicon versus Intel variants
- what native Intel users should run first
- whether the documented path already produced a distributable bundle

That ambiguity matters because macOS packaging and architecture mismatches can waste a lot of time before the real issue becomes obvious.

## Implementation

The README gained three concrete clarifications:

- explicit `./app/scripts/build_llama_macos.sh --arm64` and `--intel` examples
- a direct `make -j8 MACOS_LLAMA_INTEL` example for native Intel Macs
- an explicit statement that the documented steps do not yet produce a release-ready `.app` or `.dmg`

## Validation

Validation was documentation review only. No code or automated tests changed.

## User-visible impact

Developers building from source on macOS now have clearer expectations:

- Intel users know the shortest native path
- Apple Silicon users see the architecture split explicitly
- release packaging is correctly described as a separate step

## Remaining caveats

- This commit improved documentation, not the packaging pipeline itself.
- The repository still did not gain an automated `.app` / `.dmg` workflow here; it only became more honest about that limitation.
