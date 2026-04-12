# Diagnostic Collection Scripts

This folder includes cross-platform scripts for collecting AI File Sorter diagnostics,
automatically redacting common sensitive data, and creating a zipped bundle to share.

## Scripts

- `collect_macos_diagnostics.sh` (macOS)
- `collect_linux_diagnostics.sh` (Linux)
- `collect_windows_diagnostics.ps1` (Windows PowerShell)
- `generate_plugin_payload.sh` (Generate a publishable plugin payload; currently for the OneDrive storage plugin)
- `build_plugins_linux.sh` (Build Linux plugin targets)
- `build_plugins_macos.sh` (Build macOS plugin targets)
- `build_plugins_windows.ps1` (Build Windows plugin targets)
- `upload_plugins.py` (Upload prepared plugin payloads and merge the remote catalog)
- `verify_onedrive_windows_sync_root.ps1` (Run real Windows OneDrive sync-root verification tests)

## What they do

Each script:

1. Collects relevant app logs and platform crash/system logs.
2. Redacts common secrets and user-identifying paths from text-based files.
3. Produces a `*-redacted.zip` bundle for sharing.

## Collection window behavior

- Default: **latest-run mode**
  - Uses the newest app log file timestamp and collects around that window.
- Optional: **time-period mode**
  - You specify a duration such as `30m`, `1h`, `2h30m`, `1d`.

If no relevant app logs are found, scripts fall back to a recent window (typically 1 hour).

## macOS

Run from repo root:

```bash
./app/scripts/collect_macos_diagnostics.sh
./app/scripts/collect_macos_diagnostics.sh --time-period=1h
./app/scripts/collect_macos_diagnostics.sh --time-period=1h --open-output
```

Options:

- `--time-period=<duration>`
- `--output-dir=<path>`
- `--keep-raw`
- `--open-output`
- `-h`, `--help`

Main sources collected:

- `~/.cache/AIFileSorter/logs` (and `$XDG_CACHE_HOME/AIFileSorter/my_app/logs` if set)
- `~/Library/Logs/DiagnosticReports/*aifilesorter*`
- macOS unified logs (`log show`) filtered for AI File Sorter process/messages

## Linux

Run from repo root:

```bash
./app/scripts/collect_linux_diagnostics.sh
./app/scripts/collect_linux_diagnostics.sh --time-period=1h
./app/scripts/collect_linux_diagnostics.sh --time-period=1h --open-output
```

Options:

- `--time-period=<duration>`
- `--output-dir=<path>`
- `--keep-raw`
- `--open-output`
- `-h`, `--help`

Main sources collected:

- `~/.cache/AIFileSorter/logs` (and `$XDG_CACHE_HOME/AIFileSorter/my_app/logs` if set)
- `/var/crash` entries related to AI File Sorter (when available)
- `coredumpctl` output for `aifilesorter` (when available)
- `journalctl` entries for `aifilesorter` / `aifilesorter-bin` (when available)

## Windows (PowerShell)

Run from repo root in PowerShell:

```powershell
.\app\scripts\collect_windows_diagnostics.ps1
.\app\scripts\collect_windows_diagnostics.ps1 -TimePeriod 1h
.\app\scripts\collect_windows_diagnostics.ps1 -TimePeriod 1h -OpenOutput
```

Options:

- `-TimePeriod <duration>`
- `-OutputDir <path>`
- `-KeepRaw`
- `-OpenOutput`
- `-ShowHelp`

Main sources collected:

- `%APPDATA%\AIFileSorter\logs`
- `%LOCALAPPDATA%\CrashDumps`
- `%LOCALAPPDATA%\Microsoft\Windows\WER\ReportArchive` and `ReportQueue`
- Windows Application/System event entries matching AI File Sorter

## Output structure

Scripts write to your output directory (default desktop/home), with names like:

- `aifs-macos-diagnostics-YYYYMMDD_HHMMSS-redacted.zip`
- `aifs-linux-diagnostics-YYYYMMDD_HHMMSS-redacted.zip`
- `aifs-windows-diagnostics-YYYYMMDD_HHMMSS-redacted.zip`

Inside, the archive contains a `redacted/` folder with sanitized artifacts.

## Storage Plugin Payload

To regenerate the local OneDrive storage-plugin publish payload from the current build:

```bash
./app/scripts/generate_plugin_payload.sh \
  --base-url=https://filesorter.app/download/plugins
```

Options:

- `--base-url=<https-url>` required
- `--output-dir=<path>` optional, defaults to `plugins/storage`
- `--build-dir=<path>` optional, defaults to `build-tests`

The script writes a ready-to-upload `storage/` category tree under the local `plugins/` root and
generates URLs under `<base-url>/storage/...`.

Runtime variants are stored in per-OS/per-arch subdirectories so multiple builds can coexist:

- `plugins/storage/onedrive/linux-x86_64/...`
- `plugins/storage/onedrive/windows-x86_64/...`
- `plugins/storage/onedrive/macos-arm64/...`

Contents:

- `catalog.json`
- `onedrive/<platform>-<arch>/manifest.json`
- `onedrive/<platform>-<arch>/*.aifsplugin`
- `SHA256SUMS`

A checked-in sample catalog covering Linux, Windows, and macOS runtime variants lives at:

- `app/scripts/examples/storage/catalog.sample.json`

## Plugin Build Scripts

The plugin build scripts read their target list from:

- `app/scripts/plugin_build_targets.tsv`

Each plugin entry includes:

- internal plugin ID
- display name
- CMake target name
- output stem
- supported OS list

Examples:

```bash
./app/scripts/build_plugins_linux.sh --list
./app/scripts/build_plugins_linux.sh
./app/scripts/build_plugins_linux.sh --plugins=onedrive_storage_support
./app/scripts/build_plugins_linux.sh --interactive

./app/scripts/build_plugins_macos.sh --list
./app/scripts/build_plugins_macos.sh --interactive
```

```powershell
.\app\scripts\build_plugins_windows.ps1 -List
.\app\scripts\build_plugins_windows.ps1
.\app\scripts\build_plugins_windows.ps1 -Plugins onedrive_storage_support
.\app\scripts\build_plugins_windows.ps1 -Interactive
```

Behavior:

- default: build all plugin targets supported on the current OS
- `--list` / `-List`: list available plugin IDs for that OS
- `--plugins=...` / `-Plugins ...`: build only the requested plugin IDs
- `--interactive` / `-Interactive`: choose plugins interactively; all are selected by default

## Plugin Upload Script

To upload prepared plugin payloads from the local `plugins/` root to a remote static server tree:

```bash
python3 ./app/scripts/upload_plugins.py \
  --base-url=https://downloads.example.com/aifilesorter/plugins \
  --remote-dir=user@host:/var/www/downloads/aifilesorter/plugins
```

The uploader now treats `plugins/` as the root and handles category subtrees such as:

- `plugins/storage/...`
- `plugins/other-category/...`

Each category is expected to contain its own:

- `catalog.json`
- `SHA256SUMS`
- per-plugin runtime subdirectories containing `manifest.json`
- `.aifsplugin` archives inside those runtime subdirectories

The current OneDrive payload still lives under `plugins/storage`.

For example, if your local payload is generated under `plugins/storage`, you can upload with:

```bash
python3 ./app/scripts/upload_plugins.py \
  --base-url=https://filesorter.app/download/plugins \
  --remote-dir=user@filesorter.app:/home/webforce/filesorter_web/download/plugins \
  --dry-run
```

Useful options:

- `--list` to show prepared plugin IDs in the local payload
- `--plugins=...` to upload only selected plugin IDs
- plugin selection accepts:
  - `plugin_id` to upload all prepared runtime variants for that plugin
  - `category/plugin_id` to upload all prepared runtime variants in that category
  - `category/plugin_id/runtime` to upload one specific runtime variant
- `--interactive` to choose which prepared plugins to upload
- `--assume-empty-remote` for first-time publication
- `--dry-run` to preview the `rsync` upload
- `--allow-downgrade` to permit uploading an older version over a newer remote one
- `--force-same-version` to republish the same version when the package hash differs

What it verifies before upload:

- local manifest/package presence
- archive SHA-256 matches the manifest
- archive contains `manifest.json`, the plugin entry point, and package paths
- local build artifact exists for matching current-runtime plugin packages
- remote public catalog/manifest version and package hash, when available

What it updates on the remote:

- selected plugin `manifest.json`
- selected plugin `.aifsplugin` archives
- merged per-category `catalog.json`
- merged per-category `SHA256SUMS`

## OneDrive Windows Sync-Root Verification

To verify the authoritative Windows Cloud Files detection path against a real
OneDrive sync root on a Windows machine:

```powershell
.\app\scripts\verify_onedrive_windows_sync_root.ps1
```

or explicitly:

```powershell
.\app\scripts\verify_onedrive_windows_sync_root.ps1 `
  -BuildDir .\app\build-windows `
  -Configuration Release `
  -SyncRoot "$env:OneDrive"
```

The script:

- finds `ai_file_sorter_tests.exe`
- resolves a real OneDrive sync root from:
  - `-SyncRoot`
  - `AI_FILE_SORTER_TEST_ONEDRIVE_SYNC_ROOT`
  - `OneDrive`
- runs the two Windows-only integration tests that verify:
  - direct `OneDriveStorageProvider` detection uses `CfGetSyncRootInfoByPath`
  - the external OneDrive plugin process reports the same authoritative detection result

These tests are skipped automatically on non-Windows systems.

## Redaction notes

Redaction is best-effort and targets common sensitive patterns:

- User home paths and usernames in paths
- Authorization bearer tokens
- URL query secrets such as `key`, `api_key`, `token`, etc.
- Common API key patterns (`sk-...`, `AIza...`)

You can keep raw files with `--keep-raw` / `-KeepRaw` for local inspection,
but only share the redacted zip by default.
