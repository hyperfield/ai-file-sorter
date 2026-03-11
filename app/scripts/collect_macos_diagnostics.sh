#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Collect, redact, and zip AI File Sorter diagnostics on macOS.

Usage:
  ./collect_macos_diagnostics.sh [options]

Options:
  --time-period=<duration>   Collect logs from the last duration (e.g. 30m, 1h, 2h30m, 1d)
  --output-dir=<path>        Output directory for collected artifacts (default: ~/Desktop)
  --keep-raw                 Keep unredacted raw logs on disk (default: remove raw logs)
  --open-output              Reveal the resulting zip in Finder
  -h, --help                 Show this help

Default behavior:
  If --time-period is not supplied, the script attempts "latest run" mode:
  it finds the newest app log mtime and collects logs around that window.

Examples:
  ./collect_macos_diagnostics.sh
  ./collect_macos_diagnostics.sh --time-period=1h
  ./collect_macos_diagnostics.sh --time-period=90m --output-dir="$HOME/Desktop"
EOF
}

die() {
    echo "Error: $*" >&2
    exit 1
}

warn() {
    echo "Warning: $*" >&2
}

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

    local rest="$value"
    local total=0
    local chunk number unit multiplier

    while [[ -n "$rest" ]]; do
        if [[ "$rest" =~ ^([0-9]+)([smhd])(.*)$ ]]; then
            number="${BASH_REMATCH[1]}"
            unit="${BASH_REMATCH[2]}"
            chunk="${BASH_REMATCH[0]}"
            rest="${BASH_REMATCH[3]}"

            case "$unit" in
                s) multiplier=1 ;;
                m) multiplier=60 ;;
                h) multiplier=3600 ;;
                d) multiplier=86400 ;;
                *) die "Unsupported duration unit in '$chunk'." ;;
            esac
            total=$((total + number * multiplier))
        else
            die "Invalid duration format: '$input' (examples: 30m, 1h, 2h30m, 1d)."
        fi
    done

    (( total > 0 )) || die "Duration must be greater than zero."
    echo "$total"
}

is_text_like_file() {
    local file="$1"
    case "$file" in
        *.log|*.txt|*.json|*.crash|*.ips|*.plist|*.ini|*.cfg|*.conf|*.out|*.err)
            return 0
            ;;
    esac

    local mime
    mime="$(file -b --mime-type "$file" 2>/dev/null || true)"
    case "$mime" in
        text/*|application/json|application/xml|application/x-plist|application/x-empty)
            return 0
            ;;
    esac
    return 1
}

redact_text_file() {
    local src="$1"
    local dst="$2"
    perl -CS -pe '
        BEGIN { $home = $ENV{"HOME"} // ""; }
        if ($home ne "") {
            s/\Q$home\E/<HOME>/g;
        }
        s#(/Users/)[^/\s]+#${1}<user>#g;
        s#(/Volumes/)[^/\s]+#${1}<volume>#g;
        s#(/private/var/folders/)[^/\s]+#${1}<redacted>#g;

        s/(Authorization:\s*Bearer\s+)[^\s"\047]+/${1}<REDACTED>/ig;
        s/([?&](?:key|api_key|apikey|token|auth|authorization)=)[^&\s"\047]+/${1}<REDACTED>/ig;
        s/\bsk-(?:proj-)?[A-Za-z0-9_-]{8,}\b/sk-<REDACTED>/g;
        s/\bAIza[0-9A-Za-z_-]{20,}\b/AIza<REDACTED>/g;
    ' "$src" > "$dst"
}

copy_recent_log_files() {
    local source_dir="$1"
    local dest_dir="$2"
    local since_epoch="$3"
    local copied=0
    mkdir -p "$dest_dir"

    local file mtime
    for file in "$source_dir"/core.log* "$source_dir"/db.log* "$source_dir"/ui.log*; do
        [[ -f "$file" ]] || continue
        mtime="$(stat -f %m "$file" 2>/dev/null || echo 0)"
        if (( mtime >= since_epoch )); then
            cp "$file" "$dest_dir/"
            copied=$((copied + 1))
        fi
    done

    echo "$copied"
}

latest_app_log_epoch() {
    local latest=0
    local dir file mtime
    for dir in "${APP_LOG_DIRS[@]}"; do
        [[ -d "$dir" ]] || continue
        for file in "$dir"/core.log* "$dir"/db.log* "$dir"/ui.log*; do
            [[ -f "$file" ]] || continue
            mtime="$(stat -f %m "$file" 2>/dev/null || echo 0)"
            if (( mtime > latest )); then
                latest="$mtime"
            fi
        done
    done
    echo "$latest"
}

TIME_PERIOD_RAW=""
OUTPUT_DIR="$HOME/Desktop"
KEEP_RAW=0
OPEN_OUTPUT=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --time-period=*)
            TIME_PERIOD_RAW="${1#*=}"
            ;;
        --time-period)
            [[ -n "${2:-}" ]] || die "Missing value for --time-period"
            TIME_PERIOD_RAW="$2"
            shift
            ;;
        --output-dir=*)
            OUTPUT_DIR="${1#*=}"
            ;;
        --output-dir)
            [[ -n "${2:-}" ]] || die "Missing value for --output-dir"
            OUTPUT_DIR="$2"
            shift
            ;;
        --keep-raw)
            KEEP_RAW=1
            ;;
        --open-output)
            OPEN_OUTPUT=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown option: $1"
            ;;
    esac
    shift
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    die "This script is for macOS only."
fi

command -v perl >/dev/null 2>&1 || die "perl is required but was not found."
command -v zip >/dev/null 2>&1 || die "zip is required but was not found."
command -v log >/dev/null 2>&1 || die "macOS 'log' tool not found."

mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"

declare -a APP_LOG_DIRS
APP_LOG_DIRS=("$HOME/.cache/AIFileSorter/logs")
if [[ -n "${XDG_CACHE_HOME:-}" ]]; then
    APP_LOG_DIRS+=("$XDG_CACHE_HOME/AIFileSorter/my_app/logs")
fi

now_epoch="$(date +%s)"
since_epoch=0
window_seconds=0
window_note=""

if [[ -n "$TIME_PERIOD_RAW" ]]; then
    window_seconds="$(parse_duration_to_seconds "$TIME_PERIOD_RAW")"
    since_epoch=$((now_epoch - window_seconds))
    window_note="time-period mode (--time-period=${TIME_PERIOD_RAW})"
else
    latest_epoch="$(latest_app_log_epoch)"
    if (( latest_epoch > 0 )); then
        since_epoch=$((latest_epoch - 300))
        if (( since_epoch < 0 )); then
            since_epoch=0
        fi
        window_seconds=$((now_epoch - since_epoch))
        if (( window_seconds < 300 )); then
            window_seconds=300
        fi
        window_note="latest-run mode (newest app log mtime: $(date -r "$latest_epoch" "+%Y-%m-%d %H:%M:%S"))"
    else
        window_seconds=3600
        since_epoch=$((now_epoch - window_seconds))
        window_note="fallback mode (no app logs found, using last 1h)"
        warn "No app logs found. Falling back to last 1h."
    fi
fi

ts="$(date "+%Y%m%d_%H%M%S")"
base_name="aifs-macos-diagnostics-${ts}"
work_dir="${OUTPUT_DIR}/${base_name}"
raw_dir="${work_dir}/raw"
redacted_dir="${work_dir}/redacted"
zip_path="${OUTPUT_DIR}/${base_name}-redacted.zip"

mkdir -p "$raw_dir" "$redacted_dir"

app_logs_copied=0

if [[ -d "$HOME/.cache/AIFileSorter/logs" ]]; then
    copied="$(copy_recent_log_files "$HOME/.cache/AIFileSorter/logs" "$raw_dir/cache-logs" "$since_epoch")"
    app_logs_copied=$((app_logs_copied + copied))
fi

if [[ -n "${XDG_CACHE_HOME:-}" ]] && [[ -d "$XDG_CACHE_HOME/AIFileSorter/my_app/logs" ]]; then
    copied="$(copy_recent_log_files "$XDG_CACHE_HOME/AIFileSorter/my_app/logs" "$raw_dir/xdg-cache-logs" "$since_epoch")"
    app_logs_copied=$((app_logs_copied + copied))
fi

if (( app_logs_copied == 0 )); then
    warn "No recent app log files matched the selected window."
fi

mkdir -p "$raw_dir/DiagnosticReports"
crash_count=0
if [[ -d "$HOME/Library/Logs/DiagnosticReports" ]]; then
    while IFS= read -r -d '' crash_file; do
        mtime="$(stat -f %m "$crash_file" 2>/dev/null || echo 0)"
        if (( mtime >= since_epoch )); then
            cp "$crash_file" "$raw_dir/DiagnosticReports/"
            crash_count=$((crash_count + 1))
        fi
    done < <(find "$HOME/Library/Logs/DiagnosticReports" -maxdepth 1 -type f \
              \( -iname "*aifilesorter*" -o -iname "*AIFileSorter*" \) -print0)
fi

mkdir -p "$raw_dir/unified"
predicate='process == "aifilesorter" OR process == "AIFileSorter" OR eventMessage CONTAINS[c] "AIFileSorter" OR eventMessage CONTAINS[c] "aifilesorter"'
if ! log show --style compact --last "${window_seconds}s" --predicate "$predicate" \
    > "$raw_dir/unified/aifs_unified.log" 2> "$raw_dir/unified/aifs_unified.stderr"; then
    warn "Failed to read unified logs (see $raw_dir/unified/aifs_unified.stderr)."
fi
if [[ ! -s "$raw_dir/unified/aifs_unified.stderr" ]]; then
    rm -f "$raw_dir/unified/aifs_unified.stderr"
fi

{
    echo "Collected at: $(date "+%Y-%m-%d %H:%M:%S %z")"
    echo "Window note: ${window_note}"
    echo "Window start: $(date -r "$since_epoch" "+%Y-%m-%d %H:%M:%S")"
    echo "Window seconds: ${window_seconds}"
    echo "App logs copied: ${app_logs_copied}"
    echo "Crash reports copied: ${crash_count}"
    echo
    echo "== sw_vers =="
    sw_vers
    echo
    echo "== uname -a =="
    uname -a
    echo
    echo "== CPU =="
    sysctl -n machdep.cpu.brand_string 2>/dev/null || true
    echo
    echo "== hw.model =="
    sysctl -n hw.model 2>/dev/null || true
} > "$raw_dir/system_info.txt"

while IFS= read -r -d '' src; do
    rel="${src#$raw_dir/}"
    dst="$redacted_dir/$rel"
    mkdir -p "$(dirname "$dst")"
    if is_text_like_file "$src"; then
        redact_text_file "$src" "$dst"
    else
        cp "$src" "$dst"
    fi
done < <(find "$raw_dir" -type f -print0)

(
    cd "$work_dir"
    zip -qr "$zip_path" "redacted"
)

if (( KEEP_RAW == 0 )); then
    rm -rf "$raw_dir"
fi

echo "Diagnostics bundle ready:"
echo "  $zip_path"
echo
echo "Collected with: $window_note"
echo "If needed, inspect redacted files at:"
echo "  $redacted_dir"

if (( OPEN_OUTPUT == 1 )); then
    open -R "$zip_path" || true
fi
