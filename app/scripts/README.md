# Diagnostic Collection Scripts

This folder includes cross-platform scripts for collecting AI File Sorter diagnostics,
automatically redacting common sensitive data, and creating a zipped bundle to share.

## Scripts

- `collect_macos_diagnostics.sh` (macOS)
- `collect_linux_diagnostics.sh` (Linux)
- `collect_windows_diagnostics.ps1` (Windows PowerShell)

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

## Redaction notes

Redaction is best-effort and targets common sensitive patterns:

- User home paths and usernames in paths
- Authorization bearer tokens
- URL query secrets such as `key`, `api_key`, `token`, etc.
- Common API key patterns (`sk-...`, `AIza...`)

You can keep raw files with `--keep-raw` / `-KeepRaw` for local inspection,
but only share the redacted zip by default.
