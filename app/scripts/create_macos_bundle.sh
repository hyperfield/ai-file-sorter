#!/usr/bin/env bash

# Creates a self-contained macOS .app bundle for AI File Sorter.

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: create_macos_bundle.sh [options]

Options:
  -v, --variant <m1|m2|intel|default>  Build only the specified bundle (can repeat or comma-separate)
      --m1                            Shortcut for --variant m1
      --m2                            Shortcut for --variant m2
      --intel                         Shortcut for --variant intel
      --default                       Shortcut for --variant default
      --all                           Build all Apple Silicon variant bundles (m1, m2)
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
APP_NAME="${APP_NAME:-AIFileSorter}"
USER_APP_DISPLAY_NAME="${APP_DISPLAY_NAME-}"
APP_DISPLAY_NAME="${APP_DISPLAY_NAME:-AI File Sorter}"
APP_DIR="$ROOT_DIR/app"
BIN_DIR="${BIN_DIR:-$APP_DIR/bin}"
EXECUTABLE_SRC="${EXECUTABLE_SRC:-$BIN_DIR/aifilesorter}"
PRECOMPILED_SUBDIR="${PRECOMPILED_SUBDIR:-precompiled}"
USER_BUNDLE_OUTPUT_DIR="${BUNDLE_OUTPUT_DIR-}"
BUNDLE_OUTPUT_DIR="${BUNDLE_OUTPUT_DIR:-$APP_DIR}"
ICON_DIR="$APP_DIR/resources/images"
ICON_MASTER="$ICON_DIR/icon_512x512.png"
APP_VERSION_FILE="$APP_DIR/include/app_version.hpp"

HOST_ARCH="$(uname -m)"
BUNDLE_ARCH="${BUNDLE_ARCH:-}"
MACDEPLOYQT_OVERRIDE="${MACDEPLOYQT:-}"

detect_binary_arch() {
  local info
  if command -v lipo >/dev/null 2>&1; then
    info="$(lipo -info "$EXECUTABLE_SRC" 2>/dev/null || true)"
    if [[ "$info" == *"are:"* ]]; then
      if [[ "$info" == *"x86_64"* && "$info" == *"arm64"* ]]; then
        echo "universal"
        return
      elif [[ "$info" == *"x86_64"* ]]; then
        echo "x86_64"
        return
      elif [[ "$info" == *"arm64"* ]]; then
        echo "arm64"
        return
      fi
    elif [[ "$info" == *"architecture:"* ]]; then
      if [[ "$info" == *"x86_64"* ]]; then
        echo "x86_64"
        return
      elif [[ "$info" == *"arm64"* ]]; then
        echo "arm64"
        return
      fi
    fi
  fi
  info="$(file -b "$EXECUTABLE_SRC" 2>/dev/null || true)"
  if [[ "$info" == *"x86_64"* && "$info" == *"arm64"* ]]; then
    echo "universal"
  elif [[ "$info" == *"x86_64"* ]]; then
    echo "x86_64"
  elif [[ "$info" == *"arm64"* ]]; then
    echo "arm64"
  else
    echo ""
  fi
}

resolve_target_arch() {
  local detected
  if [[ -n "$BUNDLE_ARCH" ]]; then
    echo "$BUNDLE_ARCH"
    return
  fi
  detected="$(detect_binary_arch)"
  if [[ -n "$detected" ]]; then
    echo "$detected"
    return
  fi
  echo "$HOST_ARCH"
}

configure_bundle_paths() {
  APP_BUNDLE="$BUNDLE_OUTPUT_DIR/${APP_NAME}.app"
  MACOS_DIR="$APP_BUNDLE/Contents/MacOS"
  RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"
  FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
  PLUGINS_DIR="$APP_BUNDLE/Contents/PlugIns"
  LIB_DIR="$APP_BUNDLE/Contents/lib/${PRECOMPILED_SUBDIR}"
}

prepare_bundle_env() {
  TARGET_ARCH="$(resolve_target_arch)"
  if [[ -z "$TARGET_ARCH" ]]; then
    echo "Unable to determine target arch; defaulting to host ($HOST_ARCH)." >&2
    TARGET_ARCH="$HOST_ARCH"
  fi
  echo "Bundling for arch: $TARGET_ARCH (host: $HOST_ARCH)"
  if [[ -n "${BREW_PREFIX:-}" ]]; then
    echo "Using BREW_PREFIX override: $BREW_PREFIX"
  fi
  if [[ -n "$MACDEPLOYQT_OVERRIDE" ]]; then
    MACDEPLOYQT="$MACDEPLOYQT_OVERRIDE"
  else
    MACDEPLOYQT="$(qt_prefix qtbase)/bin/macdeployqt"
  fi
  if [[ -x "$MACDEPLOYQT" ]]; then
    local macdeployqt_info
    macdeployqt_info="$(file -b "$MACDEPLOYQT" 2>/dev/null || true)"
    if [[ "$TARGET_ARCH" != "universal" && "$macdeployqt_info" != *"$TARGET_ARCH"* ]]; then
      echo "Warning: macdeployqt arch mismatch (expected $TARGET_ARCH): $macdeployqt_info" >&2
    fi
  else
    echo "Warning: macdeployqt not found at $MACDEPLOYQT" >&2
  fi
}

brew_cmd() {
  if [[ "$TARGET_ARCH" == "x86_64" && "$HOST_ARCH" == "arm64" ]]; then
    if [[ -x /usr/local/bin/brew ]]; then
      /usr/local/bin/brew "$@"
    else
      if ! command -v brew >/dev/null 2>&1; then
        echo "x86_64 Homebrew not found. Install it under /usr/local or set BREW_PREFIX=/usr/local." >&2
        exit 1
      fi
      /usr/bin/arch -x86_64 brew "$@" || {
        echo "Failed to run brew under x86_64. Install x86_64 Homebrew or set BREW_PREFIX=/usr/local." >&2
        exit 1
      }
    fi
  elif [[ "$TARGET_ARCH" == "arm64" && "$HOST_ARCH" == "x86_64" ]]; then
    if [[ -x /opt/homebrew/bin/brew ]]; then
      /opt/homebrew/bin/brew "$@"
    else
      if ! command -v brew >/dev/null 2>&1; then
        echo "arm64 Homebrew not found. Install it under /opt/homebrew or set BREW_PREFIX=/opt/homebrew." >&2
        exit 1
      fi
      /usr/bin/arch -arm64 brew "$@" || {
        echo "Failed to run brew under arm64. Install arm64 Homebrew or set BREW_PREFIX=/opt/homebrew." >&2
        exit 1
      }
    fi
  else
    if command -v brew >/dev/null 2>&1; then
      brew "$@"
    else
      echo "brew not found. Install Homebrew or set BREW_PREFIX to the Homebrew prefix." >&2
      exit 1
    fi
  fi
}

brew_prefix() {
  local formula="$1"
  if [[ -n "${BREW_PREFIX:-}" ]]; then
    echo "${BREW_PREFIX}/opt/${formula}"
  else
    brew_cmd --prefix "$formula"
  fi
}

maybe_brew_prefix() {
  local formula="$1"
  if [[ -n "${BREW_PREFIX:-}" ]]; then
    local candidate="${BREW_PREFIX}/opt/${formula}"
    if [[ -d "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
    return 1
  fi
  if brew_cmd --prefix "$formula" >/dev/null 2>&1; then
    brew_cmd --prefix "$formula"
    return 0
  fi
  return 1
}

qt_prefix() { brew_prefix "$1"; }

# Dylibs we bundle (name|formula)
DYLIB_SPECS=$(cat <<'EOF'
libb2.1.dylib|libb2
libbrotlicommon.1.dylib|brotli
libbrotlidec.1.dylib|brotli
libcrypto.3.dylib|openssl@3
libssl.3.dylib|openssl@3
libdouble-conversion.3.dylib|double-conversion
libfmt.12.dylib|fmt
libspdlog.1.16.dylib|spdlog
libfreetype.6.dylib|freetype
libharfbuzz.0.dylib|harfbuzz
libgraphite2.3.dylib|graphite2
libpng16.16.dylib|libpng
libjsoncpp.26.dylib|jsoncpp
libmediainfo.0.dylib|libmediainfo
libzen.0.dylib|libzen
libmd4c.0.dylib|md4c
libpcre2-8.0.dylib|pcre2
libpcre2-16.0.dylib|pcre2
libzstd.1.dylib|zstd
libintl.8.dylib|gettext
libglib-2.0.0.dylib|glib
libgthread-2.0.0.dylib|glib
libdbus-1.3.dylib|dbus
libjpeg.8.dylib|jpeg-turbo
EOF
)

detect_icu_dylibs() {
  ICU_DYLIB_SPECS=""

  local qtcore
  qtcore="$(qt_prefix qtbase)/lib/QtCore.framework/Versions/A/QtCore"
  if [[ -f "$qtcore" ]] && command -v otool >/dev/null 2>&1; then
    local found=0
    local dep_path dep_name dep_formula
    while read -r dep_path; do
      dep_name="$(basename "$dep_path")"
      dep_formula="$(printf '%s\n' "$dep_path" | sed -nE 's#^.*/(opt|Cellar)/([^/]+)/.*#\2#p')"
      if [[ -z "$dep_formula" ]]; then
        dep_formula="icu4c"
      fi
      ICU_DYLIB_SPECS+="${dep_name}|${dep_formula}"$'\n'
      found=$((found + 1))
    done < <(otool -L "$qtcore" | awk '/libicu(data|i18n|uc)\.[0-9]+(\.[0-9]+)?\.dylib/ { print $1 }')
    if [[ "$found" -eq 3 ]]; then
      return 0
    fi
    ICU_DYLIB_SPECS=""
  fi

  local candidates=()
  local formulas=(icu4c@78 icu4c@77 icu4c)
  local formula prefix path base version
  for formula in "${formulas[@]}"; do
    if prefix="$(maybe_brew_prefix "$formula")"; then
      shopt -s nullglob
      for path in "$prefix/lib/libicui18n."*.dylib; do
        base="$(basename "$path")"
        version="${base#libicui18n.}"
        version="${version%.dylib}"
        candidates+=("${version}|${formula}")
      done
      shopt -u nullglob
    fi
  done
  if (( ${#candidates[@]} == 0 )); then
    return 1
  fi
  local best
  best="$(printf '%s\n' "${candidates[@]}" | sort -t'|' -k1,1V | tail -n1)"
  version="${best%%|*}"
  formula="${best#*|}"
  ICU_DYLIB_SPECS=$(cat <<EOF
libicudata.${version}.dylib|${formula}
libicui18n.${version}.dylib|${formula}
libicuuc.${version}.dylib|${formula}
EOF
)
  return 0
}

ICU_DYLIB_SPECS=""

read_app_version() {
  if [[ ! -f "$APP_VERSION_FILE" ]]; then
    echo "Version file not found at $APP_VERSION_FILE" >&2
    exit 1
  fi
  local version_line numbers major minor patch
  version_line="$(grep -E 'APP_VERSION|Version\{' "$APP_VERSION_FILE" | /usr/bin/head -n 1 || true)"
  numbers="$(printf '%s\n' "$version_line" | sed -nE 's/.*\{[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*\}.*/\1,\2,\3/p')"
  if [[ -z "$numbers" ]]; then
    echo "Unable to parse version from $APP_VERSION_FILE" >&2
    exit 1
  fi
  IFS=',' read -r major minor patch <<<"$numbers"
  if [[ -z "$major" || -z "$minor" || -z "$patch" ]]; then
    echo "Version numbers missing in $APP_VERSION_FILE (parsed: $numbers)" >&2
    exit 1
  fi
  BUNDLE_VERSION="${major}.${minor}.${patch}"
}

read_app_version

prepare_icon() {
  local iconset sizes=(16 32 64 128 256 512)
  iconset="$(mktemp -d)/${APP_NAME}.iconset"
  mkdir -p "$iconset"

  for size in "${sizes[@]}"; do
    local base="$ICON_DIR/icon_${size}x${size}.png"
    local retina_size=$((size * 2))
    local retina="$ICON_DIR/icon_${retina_size}x${retina_size}.png"
    local out_base="$iconset/icon_${size}x${size}.png"
    local out_retina="$iconset/icon_${size}x${size}@2x.png"

    if [[ -f "$base" ]]; then
      cp "$base" "$out_base"
    else
      sips -z "$size" "$size" "$ICON_MASTER" --out "$out_base" >/dev/null
    fi
    if [[ -f "$retina" ]]; then
      cp "$retina" "$out_retina"
    else
      sips -z "$retina_size" "$retina_size" "$ICON_MASTER" --out "$out_retina" >/dev/null
    fi
  done

  if ! iconutil -c icns -o "$RESOURCES_DIR/${APP_NAME}.icns" "$iconset"; then
    echo "Warning: iconutil failed to build .icns, falling back to sips conversion" >&2
    if ! sips -s format icns "$ICON_MASTER" --out "$RESOURCES_DIR/${APP_NAME}.icns" >/dev/null; then
      echo "Warning: failed to create .icns via sips; bundle will use a generic icon" >&2
    fi
  fi
  rm -rf "$(dirname "$iconset")"
}

copy_framework() {
  local formula="$1" name="$2"
  local prefix
  prefix="$(qt_prefix "$formula")"
  rsync -a "$prefix/lib/${name}.framework" "$FRAMEWORKS_DIR/"
}

copy_lib() {
  local lib="$1" formula="$2"
  local prefix file active_formula fallback_formula
  active_formula="$formula"
  prefix="$(brew_prefix "$active_formula")"
  file="$prefix/lib/$lib"
  if [[ ! -f "$file" ]]; then
    if [[ "$active_formula" == *"@"* ]]; then
      fallback_formula="${active_formula%%@*}"
      if [[ -n "$fallback_formula" ]]; then
        prefix="$(brew_prefix "$fallback_formula")"
        file="$prefix/lib/$lib"
        if [[ -f "$file" ]]; then
          active_formula="$fallback_formula"
        fi
      fi
    fi
  fi
  if [[ ! -f "$file" ]]; then
    local base alt
    base="${lib%%.*}"
    local stem family
    stem="${lib%.dylib}"
    family="${stem%.*}"

    shopt -s nullglob
    local version_matches=()
    if [[ "$family" != "$stem" ]]; then
      version_matches=("$prefix/lib/${family}."*.dylib)
    fi
    local base_matches=("$prefix/lib/${base}."*.dylib)
    shopt -u nullglob

    if (( ${#version_matches[@]} > 0 )); then
      file="${version_matches[-1]}"
    else
      alt="$prefix/lib/${base}.dylib"
      if [[ -f "$alt" ]]; then
        file="$alt"
      elif (( ${#base_matches[@]} > 0 )); then
        file="${base_matches[-1]}"
      else
        echo "Missing $lib from $formula ($file)" >&2
        exit 1
      fi
    fi
  fi
  local resolved_file actual
  resolved_file="$(realpath "$file" 2>/dev/null || printf '%s\n' "$file")"
  actual="$(basename "$resolved_file")"
  local base_name
  base_name="${lib%%.*}"
  COPIED_DYLIBS+=("${actual}|${active_formula}|${lib}|${base_name}")
  rsync -aL "$file" "$FRAMEWORKS_DIR/$actual"

  if [[ "$actual" == *.dylib ]]; then
    local prefix_name suffix compat trimmed
    prefix_name="${actual%.dylib}"
    suffix="${actual#*.}"
    suffix="${suffix%.dylib}"
    trimmed="$suffix"
    while [[ "$trimmed" == *.* ]]; do
      trimmed="${trimmed%.*}"
      compat="${prefix_name%.$suffix}.${trimmed}.dylib"
      (cd "$FRAMEWORKS_DIR" && ln -sf "$actual" "$compat")
    done
  fi
}

sign_path() {
  codesign --force --sign - "$1"
}

collect_macho_files() {
  if [[ -n "${MACHO_FILES_LOADED:-}" ]]; then
    return
  fi
  mapfile -t MACHO_FILES < <(
    while IFS= read -r -d '' candidate; do
      if file "$candidate" | grep -q 'Mach-O'; then
        printf '%s\n' "$candidate"
      fi
    done < <(find "$APP_BUNDLE" -type f -print0)
  )
  MACHO_FILES_LOADED=1
}

resolve_bundled_ref() {
  local dep="$1"
  local fw
  for fw in "${qt_frameworks[@]}"; do
    if [[ "$dep" == *"/${fw}.framework/Versions/A/${fw}" || "$dep" == "@rpath/${fw}.framework/Versions/A/${fw}" ]]; then
      printf '%s\n' "@rpath/${fw}.framework/Versions/A/${fw}"
      return 0
    fi
  done

  local base
  base="$(basename "$dep")"
  if [[ -f "$FRAMEWORKS_DIR/$base" || -f "$LIB_DIR/$base" ]]; then
    printf '%s\n' "@rpath/$base"
    return 0
  fi

  if [[ "$base" == *.dylib ]]; then
    local stem candidate
    stem="${base%.dylib}"
    shopt -s nullglob
    for candidate in "$FRAMEWORKS_DIR/${stem}"*.dylib "$LIB_DIR/${stem}"*.dylib; do
      printf '%s\n' "@rpath/$(basename "$candidate")"
      shopt -u nullglob
      return 0
    done
    shopt -u nullglob
  fi

  return 1
}

rewrite_detected_dependencies() {
  local bin="$1"
  local dep new_ref
  while read -r dep; do
    if [[ -z "$dep" ]]; then
      continue
    fi
    if new_ref="$(resolve_bundled_ref "$dep")"; then
      if [[ "$dep" != "$new_ref" ]]; then
        install_name_tool -change "$dep" "$new_ref" "$bin"
      fi
    fi
  done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}')
}

verify_no_external_homebrew_refs() {
  echo "Verifying bundle has no unresolved Homebrew library paths ..."
  collect_macho_files

  local issues=()
  local bin dep
  for bin in "${MACHO_FILES[@]}"; do
    while read -r dep; do
      if [[ "$dep" == /opt/homebrew/* || "$dep" == /usr/local/* ]]; then
        issues+=("$bin -> $dep")
      fi
    done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}')
  done

  if (( ${#issues[@]} > 0 )); then
    printf 'Unresolved external library paths remain in the bundle:\n' >&2
    printf '  %s\n' "${issues[@]}" >&2
    exit 1
  fi
}

verify_arch() {
  local expected=()
  if [[ "$TARGET_ARCH" == "universal" ]]; then
    expected=("x86_64" "arm64")
  else
    expected=("$TARGET_ARCH")
  fi

  echo "Verifying bundle architecture (${expected[*]}) ..."
  collect_macho_files

  local file info
  for file in "${MACHO_FILES[@]}"; do
    info="$(lipo -info "$file" 2>/dev/null || file -b "$file")"
    for arch in "${expected[@]}"; do
      if [[ "$info" != *"$arch"* ]]; then
        echo "Arch mismatch: $file" >&2
        echo "  Expected: $arch" >&2
        echo "  Found: $info" >&2
        exit 1
      fi
    done
  done
}

rewrite_install_names() {
  local dylibs=("${COPIED_DYLIBS[@]}")

  # Ensure bundled libraries/frameworks have @rpath install IDs
  for fw in "${qt_frameworks[@]}"; do
    chmod u+w "$FRAMEWORKS_DIR/$fw.framework/Versions/A/$fw" 2>/dev/null || true
    install_name_tool -id "@rpath/${fw}.framework/Versions/A/${fw}" "$FRAMEWORKS_DIR/$fw.framework/Versions/A/$fw"
  done
  for entry in "${dylibs[@]}"; do
    IFS='|' read -r lib _ <<<"$entry"
    chmod u+w "$FRAMEWORKS_DIR/$lib" 2>/dev/null || true
    install_name_tool -id "@rpath/$lib" "$FRAMEWORKS_DIR/$lib"
  done
  if [[ -f "$LIB_DIR/libpdfium.dylib" ]]; then
    chmod u+w "$LIB_DIR/libpdfium.dylib" 2>/dev/null || true
    install_name_tool -id "@rpath/libpdfium.dylib" "$LIB_DIR/libpdfium.dylib"
  fi

  collect_macho_files
  for bin in "${MACHO_FILES[@]}"; do
    chmod u+w "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path/../../Frameworks" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path/../Frameworks" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@executable_path/../lib/${PRECOMPILED_SUBDIR}" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path/../../lib/${PRECOMPILED_SUBDIR}" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path/../lib/${PRECOMPILED_SUBDIR}" "$bin" 2>/dev/null || true

    rewrite_detected_dependencies "$bin"
  done
}

build_bundle() {
  configure_bundle_paths
  prepare_bundle_env

  if [[ ! -f "$EXECUTABLE_SRC" ]]; then
    echo "Binary not found at $EXECUTABLE_SRC"
    echo "Run: make -C app (or set BIN_DIR/EXECUTABLE_SRC)"
    exit 1
  fi

  echo "Building bundle: $APP_DISPLAY_NAME"
  echo "Cleaning existing bundle ..."
  rm -rf "$APP_BUNDLE"

  echo "Creating bundle layout ..."
  mkdir -p "$BUNDLE_OUTPUT_DIR"
  mkdir -p "$MACOS_DIR" "$RESOURCES_DIR" "$FRAMEWORKS_DIR" "$PLUGINS_DIR" "$LIB_DIR"

  echo "Copying main executable ..."
  install -m 755 "$EXECUTABLE_SRC" "$MACOS_DIR/$APP_NAME"

  echo "Copying precompiled libraries ..."
  precompiled_src="$APP_DIR/lib/${PRECOMPILED_SUBDIR}"
  if [[ ! -d "$precompiled_src" ]]; then
    echo "Missing precompiled libraries at $precompiled_src" >&2
    exit 1
  fi
  rsync -a "$precompiled_src/" "$LIB_DIR/"

  echo "Writing Info.plist ..."
  cat > "$APP_BUNDLE/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>${APP_DISPLAY_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>com.quicknode.aifilesorter</string>
    <key>CFBundleVersion</key>
    <string>${BUNDLE_VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${BUNDLE_VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>${APP_NAME}</string>
    <key>LSMinimumSystemVersion</key>
    <string>15.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>CFBundleIconFile</key>
    <string>${APP_NAME}</string>
</dict>
</plist>
EOF

  echo "Preparing icon ..."
  if [[ -f "$ICON_MASTER" ]]; then
    prepare_icon
  else
    echo "Icon master not found at $ICON_MASTER; skipping icon generation" >&2
  fi

  echo "Copying Qt frameworks ..."
  qt_frameworks=(QtCore QtGui QtWidgets QtNetwork QtDBus)
  for fw in "${qt_frameworks[@]}"; do
    copy_framework qtbase "$fw"
  done

  echo "Copying dependency dylibs ..."
  if detect_icu_dylibs; then
    :
  else
    echo "ICU libraries not found (icu4c@78/icu4c@77/icu4c). Qt frameworks may fail to launch." >&2
    exit 1
  fi
  COPIED_DYLIBS=()
  while IFS='|' read -r lib formula; do
    if [[ -z "$lib" ]]; then
      continue
    fi
    copy_lib "$lib" "$formula"
  done <<<"${DYLIB_SPECS}"$'\n'"${ICU_DYLIB_SPECS}"

  echo "Copying Qt plugins ..."
  qt_plugins_root="$(brew_prefix qt)/share/qt/plugins"
  mkdir -p "$PLUGINS_DIR/platforms" "$PLUGINS_DIR/styles" "$PLUGINS_DIR/imageformats"
  rsync -aL "$qt_plugins_root/platforms/libqcocoa.dylib" "$PLUGINS_DIR/platforms/"
  rsync -aL "$qt_plugins_root/styles/libqmacstyle.dylib" "$PLUGINS_DIR/styles/"
  for plugin in libqjpeg.dylib libqico.dylib; do
    if [[ -f "$qt_plugins_root/imageformats/$plugin" ]]; then
      rsync -aL "$qt_plugins_root/imageformats/$plugin" "$PLUGINS_DIR/imageformats/"
    fi
  done

  echo "Removing existing code signatures ..."
  find "$APP_BUNDLE/Contents" -name "_CodeSignature" -type d -prune -exec rm -rf {} + 2>/dev/null || true

  MACHO_FILES_LOADED=""
  MACHO_FILES=()

  echo "Rewriting install names ..."
  rewrite_install_names
  verify_no_external_homebrew_refs

  verify_arch

  echo "Signing frameworks and plugins ..."
  for fw in "${qt_frameworks[@]}"; do
    sign_path "$FRAMEWORKS_DIR/$fw.framework/Versions/A/$fw"
  done
  while IFS= read -r -d '' file; do
    sign_path "$file"
  done < <(find "$FRAMEWORKS_DIR" -type f -name '*.dylib' -print0)
  while IFS= read -r -d '' file; do
    sign_path "$file"
  done < <(find "$LIB_DIR" -type f -name '*.dylib' -print0)
  while IFS= read -r -d '' file; do
    sign_path "$file"
  done < <(find "$PLUGINS_DIR" -type f -name '*.dylib' -print0)

  echo "Signing main executable and bundle ..."
  sign_path "$MACOS_DIR/$APP_NAME"
  sign_path "$APP_BUNDLE"

  echo "Deep signing bundle ..."
  codesign --force --deep --sign - "$APP_BUNDLE"

  echo "Verifying codesign ..."
  codesign --verify --deep --strict "$APP_BUNDLE"

  echo "Bundle created at: $APP_BUNDLE"
}

variants=("${REQUESTED_VARIANTS[@]}")

if (( ${#variants[@]} > 0 )); then
  echo "Building bundles: ${variants[*]}"
  for variant in "${variants[@]}"; do
    case "$variant" in
      m1)
        BIN_DIR="$APP_DIR/bin/m1"
        EXECUTABLE_SRC="$BIN_DIR/aifilesorter"
        PRECOMPILED_SUBDIR="precompiled-m1"
        BUNDLE_ARCH="arm64"
        if [[ -z "$USER_APP_DISPLAY_NAME" ]]; then
          APP_DISPLAY_NAME="AI File Sorter for Mac M1"
        fi
        ;;
      m2)
        BIN_DIR="$APP_DIR/bin/m2"
        EXECUTABLE_SRC="$BIN_DIR/aifilesorter"
        PRECOMPILED_SUBDIR="precompiled-m2"
        BUNDLE_ARCH="arm64"
        if [[ -z "$USER_APP_DISPLAY_NAME" ]]; then
          APP_DISPLAY_NAME="AI File Sorter for Mac M2-M5"
        fi
        ;;
      intel)
        BIN_DIR="$APP_DIR/bin/intel"
        EXECUTABLE_SRC="$BIN_DIR/aifilesorter"
        PRECOMPILED_SUBDIR="precompiled-intel"
        BUNDLE_ARCH="x86_64"
        if [[ -z "$USER_APP_DISPLAY_NAME" ]]; then
          APP_DISPLAY_NAME="AI File Sorter for Mac Intel"
        fi
        ;;
      default)
        # Use current env/default settings
        ;;
      *)
        echo "Unknown variant: $variant" >&2
        exit 1
        ;;
    esac
    if [[ -n "$USER_BUNDLE_OUTPUT_DIR" ]]; then
      BUNDLE_OUTPUT_DIR="$USER_BUNDLE_OUTPUT_DIR"
    elif (( ${#variants[@]} > 1 )); then
      BUNDLE_OUTPUT_DIR="$APP_DIR/dist/bundles/$variant"
    else
      BUNDLE_OUTPUT_DIR="$APP_DIR"
    fi
    build_bundle
  done
  exit 0
fi

build_bundle
