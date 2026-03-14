#!/usr/bin/env bash

set -euo pipefail

# Builds a Debian package for AI File Sorter that bundles only the project-specific
# llama/ggml libraries and assumes all other runtime libraries are supplied by the system.
#
# Usage:
#   ./package_deb.sh [options] [version]
# If no version is supplied, the script reads app/include/app_version.hpp.
# By default the package includes CPU precompiled libs. Add flags for GPU variants.

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
APP_DIR="$REPO_ROOT/app"

usage() {
    cat <<'EOF'
Usage: ./package_deb.sh [options] [version]

Options:
  --include-cuda     Include precompiled CUDA runtime libs (app/lib/precompiled/cuda)
  --include-vulkan   Include precompiled Vulkan runtime libs (app/lib/precompiled/vulkan)
  --include-all      Include CPU + CUDA + Vulkan precompiled runtime libs
  -h, --help         Show this help

Notes:
  - CPU precompiled libs are included by default.
  - Root files in app/lib/precompiled (e.g. libpdfium.so) are always included when present.
EOF
}

VERSION_FROM_HEADER() {
    local header="$1"
    if [[ ! -f "$header" ]]; then
        echo "0.0.0"
        return
    fi
    local line
    line="$(grep -m1 'APP_VERSION' "$header" || true)"
    if [[ -z "$line" ]]; then
        echo "0.0.0"
        return
    fi
    if [[ "$line" =~ Version\{[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*\} ]]; then
        printf "%s.%s.%s\n" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}"
    else
        echo "0.0.0"
    fi
}

INCLUDE_CPU=1
INCLUDE_CUDA=0
INCLUDE_VULKAN=0
VERSION_ARG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --include-cuda)
            INCLUDE_CUDA=1
            ;;
        --include-vulkan)
            INCLUDE_VULKAN=1
            ;;
        --include-all)
            INCLUDE_CPU=1
            INCLUDE_CUDA=1
            INCLUDE_VULKAN=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            if [[ -n "$VERSION_ARG" ]]; then
                echo "Unexpected extra argument: $1" >&2
                usage >&2
                exit 1
            fi
            VERSION_ARG="$1"
            ;;
    esac
    shift
done

VERSION="${VERSION_ARG:-$(VERSION_FROM_HEADER "$APP_DIR/include/app_version.hpp")}"

if [[ -z "$VERSION" ]]; then
    echo "Failed to determine package version." >&2
    exit 1
fi

BIN_PATH="$APP_DIR/bin/aifilesorter-bin"
if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary not found at $BIN_PATH — running make." >&2
    make -C "$APP_DIR"
fi

if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary still missing after build attempt." >&2
    exit 1
fi

get_needed_soname() {
    local binary="$1"
    local pattern="$2"
    if ! command -v readelf >/dev/null 2>&1; then
        return 0
    fi
    readelf -d "$binary" 2>/dev/null | awk -v pat="$pattern" '
        /NEEDED/ {
            gsub(/\[|\]/, "", $5);
            if ($5 ~ pat) { print $5; exit }
        }'
}

resolve_fmt_dep() {
    local soname
    soname="$(get_needed_soname "$BIN_PATH" '^libfmt[.]so')"
    case "$soname" in
        libfmt.so.10) echo "libfmt10" ;;
        libfmt.so.9) echo "libfmt9" ;;
        libfmt.so.8) echo "libfmt8" ;;
        *) echo "libfmt10" ;;
    esac
}

resolve_jsoncpp_dep() {
    local soname
    soname="$(get_needed_soname "$BIN_PATH" '^libjsoncpp[.]so')"
    case "$soname" in
        libjsoncpp.so.26) echo "libjsoncpp26" ;;
        libjsoncpp.so.25) echo "libjsoncpp25" ;;
        libjsoncpp.so.24) echo "libjsoncpp24" ;;
        *) echo "libjsoncpp26" ;;
    esac
}

resolve_mediainfo_dep() {
    local soname
    soname="$(get_needed_soname "$BIN_PATH" '^libmediainfo[.]so')"
    case "$soname" in
        libmediainfo.so.0) echo "libmediainfo0v5" ;;
        *) echo "libmediainfo0v5" ;;
    esac
}

join_by_comma_space() {
    local first=1
    local item
    for item in "$@"; do
        if [[ "$first" == "1" ]]; then
            printf '%s' "$item"
            first=0
        else
            printf ', %s' "$item"
        fi
    done
    printf '\n'
}

FMT_DEP="$(resolve_fmt_dep)"
JSONCPP_DEP="$(resolve_jsoncpp_dep)"
CURL_DEP="libcurl4 | libcurl4t64"
MEDIAINFO_DEP="$(resolve_mediainfo_dep)"
ZLIB_DEP="zlib1g"

OUT_DIR="$REPO_ROOT/dist/aifilesorter_deb"
PKG_NAME="aifilesorter_${VERSION}"
PKG_ROOT="$OUT_DIR/$PKG_NAME"

echo "Staging package in $PKG_ROOT"
rm -rf "$PKG_ROOT"
mkdir -p \
    "$PKG_ROOT/DEBIAN" \
    "$PKG_ROOT/opt/aifilesorter/bin" \
    "$PKG_ROOT/opt/aifilesorter/lib" \
    "$PKG_ROOT/opt/aifilesorter/certs" \
    "$PKG_ROOT/usr/bin"

install -m 0755 "$BIN_PATH" "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter-bin"
ln -sf aifilesorter-bin "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter"

PRECOMPILED_SRC="$APP_DIR/lib/precompiled"
PRECOMPILED_DST="$PKG_ROOT/opt/aifilesorter/lib/precompiled"

copy_variant_dir() {
    local variant="$1"
    local enabled="$2"
    local src_dir="$PRECOMPILED_SRC/$variant"
    if [[ "$enabled" != "1" ]]; then
        return 0
    fi
    if [[ ! -d "$src_dir" ]]; then
        echo "Requested precompiled variant '$variant' but '$src_dir' was not found." >&2
        exit 1
    fi
    cp -a "$src_dir" "$PRECOMPILED_DST/"
}

echo "Copying llama/ggml libraries"
if [[ -d "$PRECOMPILED_SRC" ]]; then
    mkdir -p "$PRECOMPILED_DST"
    # Keep root-level runtime payloads such as libpdfium.so.
    find "$PRECOMPILED_SRC" -mindepth 1 -maxdepth 1 \( -type f -o -type l \) \
        -exec cp -a {} "$PRECOMPILED_DST/" \;
    copy_variant_dir cpu "$INCLUDE_CPU"
    copy_variant_dir cuda "$INCLUDE_CUDA"
    copy_variant_dir vulkan "$INCLUDE_VULKAN"
else
    echo "Warning: '$PRECOMPILED_SRC' not found; packaging without bundled llama/ggml runtime libs." >&2
fi

SELECTED_VARIANTS=()
if [[ "$INCLUDE_CPU" == "1" ]]; then SELECTED_VARIANTS+=("cpu"); fi
if [[ "$INCLUDE_CUDA" == "1" ]]; then SELECTED_VARIANTS+=("cuda"); fi
if [[ "$INCLUDE_VULKAN" == "1" ]]; then SELECTED_VARIANTS+=("vulkan"); fi
echo "Included precompiled variants: ${SELECTED_VARIANTS[*]}"

PACKAGE_DEPENDS=(
    "libc6 (>= 2.31)"
    "libstdc++6 (>= 12)"
    "libgcc-s1 (>= 12)"
    "libqt6widgets6 (>= 6.2)"
    "libqt6gui6 (>= 6.2)"
    "libqt6core6 (>= 6.2)"
    "libqt6dbus6 (>= 6.2)"
    "qt6-wayland"
    "$CURL_DEP"
    "$JSONCPP_DEP"
    "libsqlite3-0"
    "$FMT_DEP"
    "libssl3"
    "libopenblas0-pthread"
    "$MEDIAINFO_DEP"
    "$ZLIB_DEP"
)
if [[ "$INCLUDE_VULKAN" == "1" ]]; then
    PACKAGE_DEPENDS+=("libvulkan1")
fi
PACKAGE_DEPENDS_STR="$(join_by_comma_space "${PACKAGE_DEPENDS[@]}")"

DESCRIPTION_TEXT="AI-powered file categorization tool. Requires the listed runtime libraries from the host system."
if [[ "$INCLUDE_VULKAN" == "1" ]]; then
    DESCRIPTION_TEXT+=" Includes the Vulkan backend and requires a working host Vulkan loader/driver stack."
fi
if [[ "$INCLUDE_CUDA" == "1" ]]; then
    DESCRIPTION_TEXT+=" CUDA-enabled builds require matching NVIDIA runtime libraries installed separately."
fi

if [[ -f "$APP_DIR/resources/certs/cacert.pem" ]]; then
    install -m 0644 "$APP_DIR/resources/certs/cacert.pem" "$PKG_ROOT/opt/aifilesorter/certs/cacert.pem"
fi

if [[ -f "$REPO_ROOT/LICENSE" ]]; then
    install -m 0644 "$REPO_ROOT/LICENSE" "$PKG_ROOT/opt/aifilesorter/LICENSE"
fi

cat > "$PKG_ROOT/usr/bin/run_aifilesorter.sh" <<'EOF'
#!/bin/sh
APP_DIR="/opt/aifilesorter"
CPU_LIB_DIR="$APP_DIR/lib/precompiled/cpu/bin"
CUDA_LIB_DIR="$APP_DIR/lib/precompiled/cuda/bin"
VULKAN_LIB_DIR="$APP_DIR/lib/precompiled/vulkan/bin"
PRECOMPILED_ROOT_DIR="$APP_DIR/lib/precompiled"
PLATFORM_CANDIDATES="/usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/qt6/plugins /lib/x86_64-linux-gnu/qt6/plugins"

choose_vulkan_path() {
    if [ -d "$VULKAN_LIB_DIR" ]; then
        if command -v ldconfig >/dev/null 2>&1 && ldconfig -p 2>/dev/null | grep -q libvulkan; then
            echo "$VULKAN_LIB_DIR"
            return
        fi
        for candidate in /usr/lib/x86_64-linux-gnu/libvulkan.so* /usr/lib/libvulkan.so* /lib/x86_64-linux-gnu/libvulkan.so*; do
            if [ "$candidate" = "/usr/lib/x86_64-linux-gnu/libvulkan.so*" ]; then
                break
            fi
            if [ -e "$candidate" ]; then
                echo "$VULKAN_LIB_DIR"
                return
            fi
        done
    fi
    echo ""
}

choose_cuda_path() {
    if [ -d "$CUDA_LIB_DIR" ]; then
        if command -v ldconfig >/dev/null 2>&1 && ldconfig -p 2>/dev/null | grep -q libcudart; then
            echo "$CUDA_LIB_DIR"
            return
        fi
        for candidate in /usr/local/cuda*/targets/x86_64-linux/lib/libcudart.so*; do
            if [ "$candidate" = "/usr/local/cuda*/targets/x86_64-linux/lib/libcudart.so*" ]; then
                break
            fi
            if [ -e "$candidate" ]; then
                echo "$CUDA_LIB_DIR"
                return
            fi
        done
    fi
    echo ""
}

SELECTED_VULKAN_DIR="$(choose_vulkan_path)"
SELECTED_CUDA_DIR="$(choose_cuda_path)"
PATH_COMPONENTS="$CPU_LIB_DIR:$PRECOMPILED_ROOT_DIR"
if [ -n "$SELECTED_VULKAN_DIR" ] && [ -d "$SELECTED_VULKAN_DIR" ] && [ "$SELECTED_VULKAN_DIR" != "$CPU_LIB_DIR" ]; then
    PATH_COMPONENTS="$SELECTED_VULKAN_DIR:$PATH_COMPONENTS"
elif [ -n "$SELECTED_CUDA_DIR" ] && [ -d "$SELECTED_CUDA_DIR" ] && [ "$SELECTED_CUDA_DIR" != "$CPU_LIB_DIR" ]; then
    PATH_COMPONENTS="$SELECTED_CUDA_DIR:$PATH_COMPONENTS"
fi
if [ -n "$LD_LIBRARY_PATH" ]; then
    export LD_LIBRARY_PATH="$PATH_COMPONENTS:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="$PATH_COMPONENTS"
fi

if [ -z "$QT_QPA_PLATFORM_PLUGIN_PATH" ]; then
    for candidate in $PLATFORM_CANDIDATES; do
        if [ -d "$candidate/platforms" ]; then
            export QT_QPA_PLATFORM_PLUGIN_PATH="$candidate/platforms"
            break
        fi
    done
fi

if [ -n "$QT_QPA_PLATFORM_PLUGIN_PATH" ] && [ ! -f "$QT_QPA_PLATFORM_PLUGIN_PATH/libqxcb.so" ]; then
    if [ -n "$WAYLAND_DISPLAY" ] || [ "$XDG_SESSION_TYPE" = "wayland" ]; then
        export QT_QPA_PLATFORM="wayland"
    else
        echo "Qt xcb platform plugin (libqxcb.so) not found in \$QT_QPA_PLATFORM_PLUGIN_PATH ($QT_QPA_PLATFORM_PLUGIN_PATH)." >&2
        echo "Install a Qt 6 XCB platform plugin (e.g. from qt.io archives) or run under a Wayland session." >&2
        exit 1
    fi
fi

exec "$APP_DIR/bin/aifilesorter-bin" "$@"
EOF
chmod 0755 "$PKG_ROOT/usr/bin/run_aifilesorter.sh"
ln -sf run_aifilesorter.sh "$PKG_ROOT/usr/bin/aifilesorter"

CONTROL_FILE="$PKG_ROOT/DEBIAN/control"
cat > "$CONTROL_FILE" <<EOF
Package: aifilesorter
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: AI File Sorter Team <support@example.com>
Installed-Size: 0
Depends: ${PACKAGE_DEPENDS_STR}
Description: AI File Sorter desktop application
 ${DESCRIPTION_TEXT}
EOF
chmod 0644 "$CONTROL_FILE"

echo "Adjusting permissions"
find "$PKG_ROOT" -type d -exec chmod 755 {} +
find "$PKG_ROOT/opt/aifilesorter/lib" -type f -exec chmod 0644 {} +
chmod 0755 "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter-bin"
chmod 0755 "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter"
chmod 0755 "$PKG_ROOT/usr/bin/run_aifilesorter.sh"
chmod 0755 "$PKG_ROOT/usr/bin/aifilesorter"

SIZE_KB=$(du -sk "$PKG_ROOT" | cut -f1)
sed -i "s/^Installed-Size: .*/Installed-Size: ${SIZE_KB}/" "$CONTROL_FILE"

mkdir -p "$OUT_DIR"
DEB_PATH="$OUT_DIR/${PKG_NAME}_amd64.deb"
rm -f "$DEB_PATH"

echo "Building package $DEB_PATH"
dpkg-deb --build --root-owner-group "$PKG_ROOT" "$OUT_DIR"

echo "Done. Package created at $DEB_PATH"
