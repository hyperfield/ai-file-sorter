#!/usr/bin/env bash

# Builds a drag-and-drop DMG for AI File Sorter.

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: create_dmg.sh [options]

Options:
  -v, --variant <m1|m2|intel|default>  Package only the specified bundle (can repeat or comma-separate)
      --m1                            Shortcut for --variant m1
      --m2                            Shortcut for --variant m2
      --intel                         Shortcut for --variant intel
      --default                       Shortcut for --variant default
      --all                           Package all Apple Silicon variant bundles (m1, m2)
  -h, --help                          Show this help
USAGE
}

REQUESTED_VARIANTS=()
add_variant() {
  local value="$1"
  for existing in "${REQUESTED_VARIANTS[@]}"; do
    if [[ "$existing" == "$value" ]]; then
      return
    fi
  done
  REQUESTED_VARIANTS+=("$value")
}

normalize_variant() {
  case "$1" in
    m1|M1) echo "m1" ;;
    m2|M2|m2m3|M2M3|m2-m3|M2-M3) echo "m2" ;;
    intel|Intel|x86_64|X86_64) echo "intel" ;;
    default|DEFAULT) echo "default" ;;
    all|ALL) echo "all" ;;
    *) return 1 ;;
  esac
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -v|--variant)
      if [[ -z "${2:-}" ]]; then
        echo "Missing value for --variant" >&2
        usage
        exit 1
      fi
      IFS=',' read -r -a variants <<<"$2"
      for variant in "${variants[@]}"; do
        norm="$(normalize_variant "$variant")" || {
          echo "Unknown variant: $variant" >&2
          usage
          exit 1
        }
        if [[ "$norm" == "all" ]]; then
          add_variant "m1"
          add_variant "m2"
        else
          add_variant "$norm"
        fi
      done
      shift 2
      ;;
    --m1) add_variant "m1"; shift ;;
    --m2) add_variant "m2"; shift ;;
    --intel) add_variant "intel"; shift ;;
    --default) add_variant "default"; shift ;;
    --all)
      add_variant "m1"
      add_variant "m2"
      shift
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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_DISPLAY_NAME="${APP_DISPLAY_NAME:-AI File Sorter}"
APP_BUNDLE_NAME="${APP_BUNDLE_NAME:-AIFileSorter}"
DMG_APP_DISPLAY_NAME="${DMG_APP_DISPLAY_NAME-}"
APP_DIR="$ROOT_DIR/app"
BUNDLE_SCRIPT="$APP_DIR/scripts/create_macos_bundle.sh"
DIST_DIR="$APP_DIR/dist"
BUNDLE_PATH="$APP_DIR/${APP_BUNDLE_NAME}.app"
REBUILD_BUNDLE_BEFORE_PACKAGE="${REBUILD_BUNDLE_BEFORE_PACKAGE:-1}"

rebuild_bundle_for_variant() {
  local variant="$1"
  if [[ "$REBUILD_BUNDLE_BEFORE_PACKAGE" == "0" ]]; then
    return 0
  fi
  case "$variant" in
    m1) "$BUNDLE_SCRIPT" --m1 ;;
    m2) "$BUNDLE_SCRIPT" --m2 ;;
    intel) "$BUNDLE_SCRIPT" --intel ;;
    default) "$BUNDLE_SCRIPT" --default ;;
    *)
      echo "Unknown variant: $variant" >&2
      return 1
      ;;
  esac
}

build_dmg() {
  local bundle_path="$1"
  local dmg_name="$2"
  local display_name="$3"
  local dmg_path="$DIST_DIR/$dmg_name"
  local safe_display_name="${display_name//\//-}"
  local staged_app_name="${DMG_APP_DISPLAY_NAME:-$display_name}"
  local safe_dmg_app_name="${staged_app_name//\//-}"
  if [[ "$safe_display_name" != "$display_name" ]]; then
    echo "Adjusted DMG display name for filesystem safety: $safe_display_name"
  fi
  if [[ "$safe_dmg_app_name" != "$staged_app_name" ]]; then
    echo "Adjusted staged app name for filesystem safety: $safe_dmg_app_name"
  fi

  if [[ ! -d "$bundle_path" ]]; then
    echo "App bundle not found at $bundle_path" >&2
    return 1
  fi

  mkdir -p "$DIST_DIR"
  rm -f "$dmg_path"

  local staging_dir
  staging_dir="$(mktemp -d)"
  trap 'rm -rf "$staging_dir"' EXIT

  echo "Staging DMG contents for $(basename "$bundle_path") ..."
  cp -R "$bundle_path" "$staging_dir/${safe_dmg_app_name}.app"
  ln -s /Applications "$staging_dir/Applications"

  echo "Creating DMG at $dmg_path ..."
  hdiutil create \
    -volname "$safe_display_name" \
    -srcfolder "$staging_dir" \
    -fs HFS+ \
    -format UDZO \
    "$dmg_path"

  echo "DMG created: $dmg_path"
  rm -rf "$staging_dir"
  trap - EXIT
  return 0
}

created_dmgs=()

variants=()
if (( ${#REQUESTED_VARIANTS[@]} > 0 )); then
  variants=("${REQUESTED_VARIANTS[@]}")
else
  if [[ -f "$APP_DIR/bin/m1/aifilesorter" ]]; then
    variants+=("m1")
  fi
  if [[ -f "$APP_DIR/bin/m2/aifilesorter" ]]; then
    variants+=("m2")
  fi
  if (( ${#variants[@]} == 0 )); then
    if [[ ! -d "$BUNDLE_PATH" ]]; then
      echo "No variant binaries found; default bundle missing at $BUNDLE_PATH." >&2
      echo "Run: $BUNDLE_SCRIPT" >&2
      exit 1
    fi
    variants+=("default")
  fi
fi

if (( ${#variants[@]} > 0 )); then
  echo "Packaging DMGs: ${#variants[@]}"
  for variant in "${variants[@]}"; do
    case "$variant" in
      m1)
        rebuild_bundle_for_variant "$variant"
        dmg_name="AIFileSorter-M1.dmg"
        display_name="AI File Sorter for Mac M1"
        ;;
      m2)
        rebuild_bundle_for_variant "$variant"
        dmg_name="AIFileSorter-M2-M5.dmg"
        display_name="AI File Sorter for Mac M2-M5"
        ;;
      intel)
        rebuild_bundle_for_variant "$variant"
        dmg_name="AIFileSorter-Intel.dmg"
        display_name="AI File Sorter for Mac Intel"
        ;;
      default)
        dmg_name="${APP_BUNDLE_NAME}.dmg"
        display_name="${APP_DISPLAY_NAME}"
        ;;
      *)
        echo "Unknown variant: $variant" >&2
        exit 1
        ;;
    esac
    if build_dmg "$BUNDLE_PATH" "$dmg_name" "$display_name"; then
      created_dmgs+=("$DIST_DIR/${dmg_name}")
    fi
  done
fi

if (( ${#created_dmgs[@]} == 0 )); then
  echo "No DMGs created." >&2
  exit 1
fi

echo "DMG output:"
for dmg in "${created_dmgs[@]}"; do
  echo "  - $dmg"
done
