# 2026-03-11: feat(scripts): add cross-platform diagnostics collection scripts

## Covered commits
- `02e7fc8` `2026-03-11` `feat(scripts): add cross-platform diagnostics collection scripts`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `app/scripts/collect_linux_diagnostics.sh`
- `A` `app/scripts/collect_macos_diagnostics.sh`
- `A` `app/scripts/collect_windows_diagnostics.ps1`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/scripts/collect_linux_diagnostics.sh`, `app/scripts/collect_macos_diagnostics.sh`, `app/scripts/collect_windows_diagnostics.ps1`. It changed the project from not having the capability described by `feat(scripts): add cross-platform diagnostics collection scripts` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `02e7fc8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/collect_linux_diagnostics.sh b/app/scripts/collect_linux_diagnostics.sh
--- /dev/null
+++ b/app/scripts/collect_linux_diagnostics.sh
@@ -0,0 +1,363 @@
+#!/usr/bin/env bash
+set -euo pipefail
+
+usage() {
+    cat <<'EOF'
+Collect, redact, and zip AI File Sorter diagnostics on Linux.
+
+Usage:
+  ./collect_linux_diagnostics.sh [options]
+
+Options:
+  --time-period=<duration>   Collect logs from the last duration (e.g. 30m, 1h, 2h30m, 1d)
+  --output-dir=<path>        Output directory for collected artifacts (default: ~/Desktop or ~)
+  --keep-raw                 Keep unredacted raw logs on disk (default: remove raw logs)
+  --open-output              Open output directory after completion
+  -h, --help                 Show this help
+
+Default behavior:
+  If --time-period is not supplied, the script attempts "latest run" mode:
+  it finds the newest app log mtime and collects logs around that window.
+
+Examples:
+  ./collect_linux_diagnostics.sh
+  ./collect_linux_diagnostics.sh --time-period=1h
+  ./collect_linux_diagnostics.sh --time-period=90m --output-dir="$HOME/Desktop"
+EOF
+}
+
+die() {
+    echo "Error: $*" >&2
+    exit 1
+}
+
+warn() {
+    echo "Warning: $*" >&2
+}
+
+parse_duration_to_seconds() {
+    local input="$1"
+    local value
+    value="$(printf '%s' "$input" | tr '[:upper:]' '[:lower:]')"
+    value="${value// /}"
+    [[ -n "$value" ]] || die "Empty duration."
+
+    if [[ "$value" =~ ^[0-9]+$ ]]; then
+        echo "$value"
+        return
+    fi
+
+    local rest="$value"
+    local total=0
+    local chunk number unit multiplier
+
+    while [[ -n "$rest" ]]; do
+        if [[ "$rest" =~ ^([0-9]+)([smhd])(.*)$ ]]; then
+            number="${BASH_REMATCH[1]}"
+            unit="${BASH_REMATCH[2]}"
+            chunk="${BASH_REMATCH[0]}"
+            rest="${BASH_REMATCH[3]}"
+
+            case "$unit" in
+                s) multiplier=1 ;;
+                m) multiplier=60 ;;
+                h) multiplier=3600 ;;
+                d) multiplier=86400 ;;
+                *) die "Unsupported duration unit in '$chunk'." ;;
+            esac
+            total=$((total + number * multiplier))
+        else
+            die "Invalid duration format: '$input' (examples: 30m, 1h, 2h30m, 1d)."
+        fi
+    done
+
+    (( total > 0 )) || die "Duration must be greater than zero."
+    echo "$total"
+}
+
+is_text_like_file() {
+    local file="$1"
+    case "$file" in
+        *.log|*.txt|*.json|*.crash|*.ips|*.ini|*.cfg|*.conf|*.out|*.err)
+            return 0
+            ;;
+    esac
+
+    local mime
+    mime="$(file -b --mime-type "$file" 2>/dev/null || true)"
+    case "$mime" in
+        text/*|application/json|application/xml|application/x-empty)
+            return 0
+            ;;
+    esac
+    return 1
+}
+
+redact_text_file() {
```

The excerpt is taken from the commit diff for `feat(scripts): add cross-platform diagnostics collection scripts`. The most relevant surfaces are `app/scripts/collect_linux_diagnostics.sh`, `app/scripts/collect_macos_diagnostics.sh`, `app/scripts/collect_windows_diagnostics.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
