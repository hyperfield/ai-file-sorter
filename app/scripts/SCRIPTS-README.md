# Scripts README

## build_llama_macos.sh

Builds the llama.cpp dynamic libraries used by the app on macOS.

Usage:

```bash
./app/scripts/build_llama_macos.sh [options]
```

Options:
- `--arch <arm64>`: Target architecture.
- `--arm64`, `--m1`, `--m2`, `--m3`: Alias for `--arch arm64`.
- `-h`, `--help`: Show help.

Notes:
- `--m1` selects `arm64` (same as any Apple Silicon build). For an M1-safe CPU backend, also set `LLAMA_MACOS_MULTI_VARIANT=1`.

Environment overrides:
- `LLAMA_MACOS_ARCH`: Target architecture. CLI flags take precedence.
- `LLAMA_MACOS_ENABLE_METAL=0|1|auto`: Enable Metal backend. Default is `auto` for Apple Silicon builds.
- `LLAMA_MACOS_MULTI_VARIANT=0|1`: Build multi-variant CPU backends (useful for M1-safe binaries).
- `LLAMA_PRECOMPILED_DIR`: Output directory for the compiled libs (default: `app/lib/precompiled`).
- `MACOSX_DEPLOYMENT_TARGET`: Override the deployment target (default: `15.0`).

Examples:

```bash
# Apple Silicon (arm64)
./app/scripts/build_llama_macos.sh --arm64

# M1-safe multi-variant CPU backend
LLAMA_MACOS_MULTI_VARIANT=1 ./app/scripts/build_llama_macos.sh --arm64
```

Outputs:
- Precompiled libraries are staged in `app/lib/precompiled/` (or `LLAMA_PRECOMPILED_DIR` if set).
- Updated headers are staged in `app/include/llama/`.

## create_macos_bundle.sh

Creates one or more macOS `.app` bundles.

Default output:
- Single-variant runs write `AIFileSorter.app` under `app/`.
- Multi-variant runs such as `--all` write `AIFileSorter.app` into variant-specific folders under `app/dist/bundles/`.

Variant mapping:
- `m1` uses `app/bin/m1/aifilesorter`
- `m2` uses `app/bin/m2/aifilesorter`
- `default` uses `app/bin/aifilesorter`

Usage:

```bash
./app/scripts/create_macos_bundle.sh
```

CLI flags (optional):
- `-v, --variant <m1|m2|default>`: Build only the specified bundle (repeat or comma-separate).
- `--m1`, `--m2`, `--default`: Shortcuts for `--variant`.
- `--all`: Build all Apple Silicon variant bundles.

Environment overrides (optional):
- `APP_NAME`: Bundle name (default: `AIFileSorter`).
- `APP_DISPLAY_NAME`: Display name (default: `AI File Sorter`).
- `BIN_DIR`: Source binary directory (default: `app/bin`).
- `EXECUTABLE_SRC`: Full path to the source binary (default: `$BIN_DIR/aifilesorter`).
- `PRECOMPILED_SUBDIR`: Subdirectory under `app/lib/` to bundle from (default: `precompiled`).
- `BUNDLE_OUTPUT_DIR`: Bundle output directory (default: `app/` for single-variant runs).
- `BUNDLE_ARCH`: Force target arch (`arm64` or `universal`).
- `BREW_PREFIX`: Override Homebrew prefix.
- `MACDEPLOYQT`: Full path to `macdeployqt` (optional; used for arch sanity checks).

## create_dmg.sh

Packages macOS `.app` bundles into `.dmg` files.

Behavior:
- Variant runs rebuild `AIFileSorter.app` for the requested variant first, unless `REBUILD_BUNDLE_BEFORE_PACKAGE=0` is set.
- Variant DMGs are written with distinct filenames under `app/dist/`.
- If no variant is requested, the script packages the existing `app/AIFileSorter.app` bundle when present.

Usage:

```bash
./app/scripts/create_dmg.sh
```

CLI flags (optional):
- `-v, --variant <m1|m2|default>`: Package only the specified bundle (repeat or comma-separate).
- `--m1`, `--m2`, `--default`: Shortcuts for `--variant`.
- `--all`: Package all Apple Silicon variant bundles.

Output:
- Creates one DMG per bundle under `app/dist/` and prints the list at the end.

## vendor_doc_deps.sh

Downloads and stages third-party document analysis dependencies (libzip, pugixml, pdfium).

Usage:

```bash
./app/scripts/vendor_doc_deps.sh
```

Environment overrides:
- `LIBZIP_VERSION` (default: `1.11.4`)
- `PUGIXML_VERSION` (default: `1.15`)
- `PDFIUM_RELEASE` (default: `latest`)
- `PDFIUM_MAC_X64_TGZ` (default: `pdfium-mac-x64.tgz`)

Output:
- Writes into `external/libzip/`, `external/pugixml/`, and `external/pdfium/`.
- Licenses copied into `external/THIRD_PARTY_LICENSES/`.
