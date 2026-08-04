# 2026-03-11: docs(scripts): add usage guide for diagnostics collection scripts

## Covered commits
- `f65dff7` `2026-03-11` `docs(scripts): add usage guide for diagnostics collection scripts`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `app/scripts/README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/scripts/README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `f65dff7`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/README.md b/app/scripts/README.md
--- /dev/null
+++ b/app/scripts/README.md
@@ -0,0 +1,123 @@
+# Diagnostic Collection Scripts
+
+This folder includes cross-platform scripts for collecting AI File Sorter diagnostics,
+automatically redacting common sensitive data, and creating a zipped bundle to share.
+
+## Scripts
+
+- `collect_macos_diagnostics.sh` (macOS)
+- `collect_linux_diagnostics.sh` (Linux)
+- `collect_windows_diagnostics.ps1` (Windows PowerShell)
+
+## What they do
+
+Each script:
+
+1. Collects relevant app logs and platform crash/system logs.
+2. Redacts common secrets and user-identifying paths from text-based files.
+3. Produces a `*-redacted.zip` bundle for sharing.
+
+## Collection window behavior
+
+- Default: **latest-run mode**
+  - Uses the newest app log file timestamp and collects around that window.
+- Optional: **time-period mode**
+  - You specify a duration such as `30m`, `1h`, `2h30m`, `1d`.
+
+If no relevant app logs are found, scripts fall back to a recent window (typically 1 hour).
+
+## macOS
+
+Run from repo root:
+
+```bash
+./app/scripts/collect_macos_diagnostics.sh
+./app/scripts/collect_macos_diagnostics.sh --time-period=1h
+./app/scripts/collect_macos_diagnostics.sh --time-period=1h --open-output
+```
+
+Options:
+
+- `--time-period=<duration>`
+- `--output-dir=<path>`
+- `--keep-raw`
+- `--open-output`
+- `-h`, `--help`
+
+Main sources collected:
+
+- `~/.cache/AIFileSorter/logs` (and `$XDG_CACHE_HOME/AIFileSorter/my_app/logs` if set)
+- `~/Library/Logs/DiagnosticReports/*aifilesorter*`
+- macOS unified logs (`log show`) filtered for AI File Sorter process/messages
+
+## Linux
+
+Run from repo root:
+
+```bash
+./app/scripts/collect_linux_diagnostics.sh
+./app/scripts/collect_linux_diagnostics.sh --time-period=1h
+./app/scripts/collect_linux_diagnostics.sh --time-period=1h --open-output
+```
+
+Options:
+
+- `--time-period=<duration>`
+- `--output-dir=<path>`
+- `--keep-raw`
+- `--open-output`
+- `-h`, `--help`
+
+Main sources collected:
+
+- `~/.cache/AIFileSorter/logs` (and `$XDG_CACHE_HOME/AIFileSorter/my_app/logs` if set)
+- `/var/crash` entries related to AI File Sorter (when available)
+- `coredumpctl` output for `aifilesorter` (when available)
+- `journalctl` entries for `aifilesorter` / `aifilesorter-bin` (when available)
+
+## Windows (PowerShell)
+
+Run from repo root in PowerShell:
+
+```powershell
+.\app\scripts\collect_windows_diagnostics.ps1
+.\app\scripts\collect_windows_diagnostics.ps1 -TimePeriod 1h
+.\app\scripts\collect_windows_diagnostics.ps1 -TimePeriod 1h -OpenOutput
+```
+
+Options:
+
+- `-TimePeriod <duration>`
+- `-OutputDir <path>`
+- `-KeepRaw`
+- `-OpenOutput`
+- `-ShowHelp`
+
+Main sources collected:
```

The excerpt is taken from the commit diff for `docs(scripts): add usage guide for diagnostics collection scripts`. The most relevant surfaces are `app/scripts/README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
