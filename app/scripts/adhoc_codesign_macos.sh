#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -lt 1 ]]; then
    echo "Usage: $0 <mach-o-file> [<mach-o-file> ...]" >&2
    exit 1
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This helper is for macOS only." >&2
    exit 1
fi

if ! command -v codesign >/dev/null 2>&1; then
    echo "Error: codesign is required to ad-hoc sign macOS binaries." >&2
    exit 1
fi

for target in "$@"; do
    if [[ ! -e "$target" ]]; then
        echo "Error: '$target' does not exist." >&2
        exit 1
    fi

    # Sign the real Mach-O file, not versioned symlink aliases.
    if [[ -L "$target" ]]; then
        continue
    fi

    chmod u+w "$target" 2>/dev/null || true
    codesign --force --sign - --timestamp=none "$target"
done
