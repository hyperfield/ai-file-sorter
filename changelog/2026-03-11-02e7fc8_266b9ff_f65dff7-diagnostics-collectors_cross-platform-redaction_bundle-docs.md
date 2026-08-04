# 2026-03-11: Cross-platform diagnostics collectors, automatic redaction, and script documentation

## Scope

This entry documents the diagnostics tooling added on 2026-03-11:

- `app/scripts/collect_macos_diagnostics.sh`
- `app/scripts/collect_linux_diagnostics.sh`
- `app/scripts/collect_windows_diagnostics.ps1`
- `app/scripts/README.md`

The change set introduces a unified support workflow for macOS, Linux, and Windows:

1. collect logs for a relevant time window,
2. redact common sensitive data patterns,
3. zip a redacted bundle ready to share.

## Motivation

### 1) Manual support collection was too error-prone

Before this change, diagnostics collection depended on manually sending multiple platform-specific commands and asking users to run them in the right order. This created avoidable problems:

- users missing one required source (app logs, crash logs, system logs),
- users collecting stale logs from old runs instead of the incident in question,
- users sending raw logs without any sanitization.

A one-command script per platform reduces support friction and improves consistency.

### 2) Time relevance needed to be first-class

Most troubleshooting requests need logs from *either*:

- the latest run that failed, or
- a precise recent time period (for example, “last 1 hour”).

The scripts therefore support both:

- **default latest-run mode** (derived from newest app log mtime),
- **explicit time-period mode** (`1h`, `30m`, `2h30m`, `1d`).

### 3) Privacy-by-default was required

App logs can include filenames, directory paths, and occasionally content-adjacent text. Even when API keys are not usually logged directly, path and token redaction is still justified to avoid accidental exposure in support channels.

The scripts now produce **redacted zips by default**, while still allowing local retention of raw artifacts when needed.

## What changed (from what -> to what)

### A) macOS collector (`collect_macos_diagnostics.sh`)

#### Before

- No dedicated script to gather AI File Sorter diagnostics on macOS.
- Support had to provide ad-hoc command lists per user.

#### After

- Added a dedicated macOS collector script with:
  - `--time-period=<duration>`,
  - `--output-dir=<path>`,
  - `--keep-raw`,
  - `--open-output`.
- Collects app logs from:
  - `~/.cache/AIFileSorter/logs`,
  - `$XDG_CACHE_HOME/AIFileSorter/my_app/logs` (if present).
- Collects crash reports from:
  - `~/Library/Logs/DiagnosticReports/*aifilesorter*`.
- Collects unified logs via `log show` with AI File Sorter predicates.
- Redacts text artifacts and creates a `*-redacted.zip` bundle.

#### Relevant excerpt (duration parsing)

```bash
parse_duration_to_seconds() {
    local input="$1"
    local value
    value="$(printf '%s' "$input" | tr '[:upper:]' '[:lower:]')"
    value="${value// /}"
    [[ -n "$value" ]] || die "Empty duration."

    if [[ "$value" =~ ^[0-9]+$ ]]; then
        echo "$value"
        return
    fi

    # Comment: supports chained forms like 2h30m and normalizes to seconds.
    while [[ -n "$rest" ]]; do
        if [[ "$rest" =~ ^([0-9]+)([smhd])(.*)$ ]]; then
            ...
        else
            die "Invalid duration format ..."
        fi
    done
}
```

#### Relevant excerpt (latest-run default)

```bash
latest_epoch="$(latest_app_log_epoch)"
if (( latest_epoch > 0 )); then
    since_epoch=$((latest_epoch - 300))
    window_note="latest-run mode (newest app log mtime: ...)"
else
    window_seconds=3600
    since_epoch=$((now_epoch - window_seconds))
    window_note="fallback mode (no app logs found, using last 1h)"
fi
```

### B) Linux collector (`collect_linux_diagnostics.sh`)

#### Before

- No dedicated Linux diagnostics script; support relied on manual commands.

#### After

- Added Linux collector script with the same option model as macOS.
- Collects app logs from XDG cache locations.
- Collects Linux-native crash/system artifacts when available:
  - `/var/crash` files matching app naming,
  - `coredumpctl` listing for `aifilesorter`,
  - `journalctl` entries for `aifilesorter` / `aifilesorter-bin`.
- Falls back gracefully when tools are unavailable (`journalctl`, `coredumpctl`).
- Produces redacted zip output identical in workflow to macOS.

#### Relevant excerpt (journal/coredump collection)

```bash
if command -v coredumpctl >/dev/null 2>&1; then
    coredumpctl --since "@${since_epoch}" list aifilesorter \
        > "$raw_dir/crash/coredumpctl_aifilesorter.txt" 2>&1 || true
fi

if command -v journalctl >/dev/null 2>&1; then
    journalctl --no-pager --since "@${since_epoch}" _COMM=aifilesorter \
        > "$raw_dir/journal/journal_aifilesorter.log" 2> ...
    journalctl --no-pager --since "@${since_epoch}" _COMM=aifilesorter-bin \
        > "$raw_dir/journal/journal_aifilesorter_bin.log" 2> ...
fi
```

#### Relevant excerpt (Linux redaction additions)

```bash
perl -CS -pe '
    # Comment: replace common Linux user-identifying path segments.
    s#(/home/)[^/\s]+#${1}<user>#g;
    s#(/media/)[^/\s]+#${1}<media>#g;
    s#(/mnt/)[^/\s]+#${1}<mount>#g;
    s#(/run/user/)[0-9]+#${1}<uid>#g;
    ...
' "$src" > "$dst"
```

### C) Windows collector (`collect_windows_diagnostics.ps1`)

#### Before

- No PowerShell collector script for support-safe diagnostics packaging.

#### After

- Added `collect_windows_diagnostics.ps1` with PowerShell-native parameters:
  - `-TimePeriod`,
  - `-OutputDir`,
  - `-KeepRaw`,
  - `-OpenOutput`,
  - `-ShowHelp`.
- Collects app logs from `%APPDATA%\AIFileSorter\logs`.
- Collects crash artifacts from:
  - `%LOCALAPPDATA%\CrashDumps`,
  - `%LOCALAPPDATA%\Microsoft\Windows\WER\ReportArchive`,
  - `%LOCALAPPDATA%\Microsoft\Windows\WER\ReportQueue`.
- Collects Event Viewer matches from `Application` and `System`.
- Applies token/path redaction to text files and zips redacted output.

#### Relevant excerpt (time period parsing in PowerShell)

```powershell
function Convert-TimePeriodToTimeSpan {
    param([string]$InputValue)

    # Comment: accepts 30m / 1h / 2h30m / 1d style durations.
    $m = [regex]::Match($rest, '^([0-9]+)([smhd])(.*)$')
    switch ($unit) {
        "s" { $totalSeconds += $number }
        "m" { $totalSeconds += $number * 60 }
        "h" { $totalSeconds += $number * 3600 }
        "d" { $totalSeconds += $number * 86400 }
    }
    return [TimeSpan]::FromSeconds($totalSeconds)
}
```

#### Relevant excerpt (event log collection)

```powershell
$appEvents = Get-WinEvent -FilterHashtable @{ LogName = "Application"; StartTime = $since } |
    Where-Object { $_.Message -match '(?i)aifilesorter|AIFileSorter' }

$sysEvents = Get-WinEvent -FilterHashtable @{ LogName = "System"; StartTime = $since } |
    Where-Object { $_.Message -match '(?i)aifilesorter|AIFileSorter' }

# Comment: Event logs are often the only reliable source for startup/runtime failures on Windows.
```

### D) Script-level documentation (`app/scripts/README.md`)

#### Before

- No dedicated scripts README documenting diagnostics collectors.

#### After

- Added `app/scripts/README.md` covering:
  - all three scripts,
  - common behavior,
  - options and platform-specific sources,
  - output format,
  - redaction expectations.

This reduces onboarding/support ambiguity and avoids relying on tribal knowledge.

## Redaction model and rationale

All three scripts redact common high-risk text patterns in text-like files:

- user home paths and username segments in OS-specific path formats,
- bearer tokens in authorization headers,
- URL query credentials (`key`, `api_key`, `token`, etc.),
- common OpenAI/Google key patterns (`sk-*`, `AIza*`).

Why this model:

- It catches frequent accidental leakage vectors quickly.
- It preserves log utility better than blanket deletion.
- It can be applied uniformly across all text artifacts without requiring users to manually edit files.

Limitations (explicitly accepted tradeoff):

- Redaction is **best-effort**, not formal data-loss-prevention.
- Non-standard secret formats may remain if they do not match heuristics.
- Binary dumps are copied as-is (intended; redaction targets text files).

## Operational behavior details

### Output naming and structure

Each script creates an output set with:

- timestamped working folder,
- `raw/` collected artifacts,
- `redacted/` sanitized artifacts,
- `*-redacted.zip` archive containing `redacted/`.

The default sharing artifact is the redacted zip.

### “Latest run” behavior

Latest-run mode is intentionally derived from app log mtimes because:

- it requires no PID/session tracking,
- it works across normal crash/restart workflows,
- it is robust even when users run the app via different launchers.

Implementation detail:

- scripts subtract a small pre-window buffer (5 minutes) from newest log mtime to capture startup context and pre-failure warnings.

## Validation performed during implementation

### macOS script

- `bash -n app/scripts/collect_macos_diagnostics.sh`
- `app/scripts/collect_macos_diagnostics.sh --help`

### Linux script

- `bash -n app/scripts/collect_linux_diagnostics.sh`
- `app/scripts/collect_linux_diagnostics.sh --help`
- dry run:
  - `app/scripts/collect_linux_diagnostics.sh --time-period=1m --output-dir=<tmp> --keep-raw`
  - verified redacted zip creation path.

### Windows script

- PowerShell help/parse invocation succeeded in this environment:
  - `.\app\scripts\collect_windows_diagnostics.ps1 -ShowHelp`
- full Windows-native artifact collection was not executed in this Linux environment.

## Outcome

This change provides a coherent, low-friction support diagnostics pipeline across all supported desktop platforms. It materially improves:

- data quality (time-scoped, relevant logs),
- response speed (single-command collection),
- privacy baseline (automatic redaction before sharing),
- and maintainability (documented scripts and common behavior model).

