#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: verify_macos_release.sh --bundle <path> --dmg <path> [--app-display-name <name>]

Checks that the macOS bundle and the app staged inside the DMG both match the
version declared in app/include/app_version.hpp and use the canonical app names.
USAGE
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION_FILE="$ROOT_DIR/app/include/app_version.hpp"
BUNDLE_PATH=""
DMG_PATH=""
APP_DISPLAY_NAME="AI File Sorter"
MOUNT_POINT=""
SUPPORTED_MIN_MACOS_VERSION="${SUPPORTED_MIN_MACOS_VERSION:-${MACOSX_DEPLOYMENT_TARGET:-15.0}}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bundle)
      BUNDLE_PATH="${2:-}"
      shift 2
      ;;
    --dmg)
      DMG_PATH="${2:-}"
      shift 2
      ;;
    --app-display-name)
      APP_DISPLAY_NAME="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$BUNDLE_PATH" || -z "$DMG_PATH" ]]; then
  usage
  exit 1
fi

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "Version file not found at $VERSION_FILE" >&2
  exit 1
fi

EXPECTED_VERSION="$(
  grep -E 'APP_VERSION|Version\{' "$VERSION_FILE" | /usr/bin/head -n 1 \
    | sed -nE 's/.*\{[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*\}.*/\1.\2.\3/p'
)"

if [[ -z "$EXPECTED_VERSION" ]]; then
  echo "Unable to parse version from $VERSION_FILE" >&2
  exit 1
fi

plist_value() {
  local plist="$1"
  local key="$2"
  /usr/libexec/PlistBuddy -c "Print :$key" "$plist"
}

version_gt() {
  local lhs="$1"
  local rhs="$2"
  awk -v lhs="$lhs" -v rhs="$rhs" '
    BEGIN {
      split(lhs, a, ".");
      split(rhs, b, ".");
      for (i = 1; i <= 3; ++i) {
        av = (i in a && a[i] != "") ? a[i] + 0 : 0;
        bv = (i in b && b[i] != "") ? b[i] + 0 : 0;
        if (av > bv) {
          exit 0;
        }
        if (av < bv) {
          exit 1;
        }
      }
      exit 1;
    }'
}

extract_macos_min_version() {
  local mach_o="$1"
  otool -l "$mach_o" 2>/dev/null | awk '
    $1 == "cmd" && $2 == "LC_BUILD_VERSION" {
      mode = "build";
      next;
    }
    $1 == "cmd" && $2 == "LC_VERSION_MIN_MACOSX" {
      mode = "legacy";
      next;
    }
    mode == "build" && $1 == "minos" {
      print $2;
      exit;
    }
    mode == "legacy" && $1 == "version" {
      print $2;
      exit;
    }
    $1 == "cmd" {
      mode = "";
    }'
}

verify_bundle_minimum_macos_version() {
  local bundle="$1"
  local location="$2"
  local issues=()
  local mach_o info min_version relative

  while IFS= read -r -d '' mach_o; do
    info="$(file -b "$mach_o" 2>/dev/null || true)"
    if [[ "$info" != *"Mach-O"* ]]; then
      continue
    fi

    min_version="$(extract_macos_min_version "$mach_o")"
    if [[ -z "$min_version" ]]; then
      relative="${mach_o#"$bundle"/}"
      issues+=("${relative}: missing macOS deployment metadata")
      continue
    fi

    if version_gt "$min_version" "$SUPPORTED_MIN_MACOS_VERSION"; then
      relative="${mach_o#"$bundle"/}"
      issues+=("${relative}: built for macOS ${min_version}")
    fi
  done < <(find "$bundle/Contents" -type f -print0)

  if (( ${#issues[@]} > 0 )); then
    printf '%s bundle requires a newer macOS release than the supported floor %s:\n' \
      "$location" "$SUPPORTED_MIN_MACOS_VERSION" >&2
    printf '  %s\n' "${issues[@]}" >&2
    exit 1
  fi
}

verify_bundle() {
  local bundle="$1"
  local location="$2"
  local expected_bundle_name="$3"
  local plist="$bundle/Contents/Info.plist"

  if [[ ! -d "$bundle" ]]; then
    echo "$location bundle not found at $bundle" >&2
    exit 1
  fi
  if [[ "$(basename "$bundle")" != "$expected_bundle_name" ]]; then
    echo "$location bundle name mismatch: expected $expected_bundle_name, found $(basename "$bundle")" >&2
    exit 1
  fi
  if [[ ! -f "$plist" ]]; then
    echo "$location Info.plist not found at $plist" >&2
    exit 1
  fi

  local bundle_name bundle_display_name bundle_version short_version
  bundle_name="$(plist_value "$plist" CFBundleName)"
  bundle_display_name="$(plist_value "$plist" CFBundleDisplayName)"
  bundle_version="$(plist_value "$plist" CFBundleVersion)"
  short_version="$(plist_value "$plist" CFBundleShortVersionString)"

  if [[ "$bundle_name" != "AIFileSorter" ]]; then
    echo "$location CFBundleName mismatch: expected AIFileSorter, found $bundle_name" >&2
    exit 1
  fi
  if [[ "$bundle_display_name" != "$APP_DISPLAY_NAME" ]]; then
    echo "$location CFBundleDisplayName mismatch: expected $APP_DISPLAY_NAME, found $bundle_display_name" >&2
    exit 1
  fi
  if [[ "$bundle_version" != "$EXPECTED_VERSION" ]]; then
    echo "$location CFBundleVersion mismatch: expected $EXPECTED_VERSION, found $bundle_version" >&2
    exit 1
  fi
  if [[ "$short_version" != "$EXPECTED_VERSION" ]]; then
    echo "$location CFBundleShortVersionString mismatch: expected $EXPECTED_VERSION, found $short_version" >&2
    exit 1
  fi

  verify_bundle_minimum_macos_version "$bundle" "$location"
}

cleanup_mount() {
  if [[ -n "$MOUNT_POINT" && -d "$MOUNT_POINT" ]]; then
    hdiutil detach "$MOUNT_POINT" >/dev/null 2>&1 || true
    rm -rf "$MOUNT_POINT"
  fi
}

trap cleanup_mount EXIT

verify_bundle "$BUNDLE_PATH" "Bundle" "AIFileSorter.app"

if [[ ! -f "$DMG_PATH" ]]; then
  echo "DMG not found at $DMG_PATH" >&2
  exit 1
fi

MOUNT_POINT="$(mktemp -d)"
hdiutil attach -nobrowse -readonly -mountpoint "$MOUNT_POINT" "$DMG_PATH" >/dev/null
verify_bundle "$MOUNT_POINT/${APP_DISPLAY_NAME}.app" "DMG" "${APP_DISPLAY_NAME}.app"

echo "Verified macOS release bundle and DMG against version $EXPECTED_VERSION and macOS floor $SUPPORTED_MIN_MACOS_VERSION."
