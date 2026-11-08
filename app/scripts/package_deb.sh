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
  --cpu-only         Include only the CPU runtime, even if GPU variants are staged.
  --include-cuda     Include precompiled CUDA runtime libs (app/lib/precompiled/cuda)
  --include-vulkan   Include precompiled Vulkan runtime libs (app/lib/precompiled/vulkan)
  --include-all      Include CPU + CUDA + Vulkan precompiled runtime libs
  -h, --help         Show this help

Notes:
  - CPU precompiled libs are included by default.
  - Staged CUDA/Vulkan runtime dirs are auto-included when present unless --cpu-only is used.
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
    if [[ "$line" =~ \{[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*\} ]]; then
        printf "%s.%s.%s\n" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}"
    else
        echo "0.0.0"
    fi
}

INCLUDE_CPU=1
AUTO_INCLUDE_GPU=1
REQUIRE_CUDA=0
REQUIRE_VULKAN=0
VERSION_ARG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cpu-only)
            AUTO_INCLUDE_GPU=0
            ;;
        --include-cuda)
            REQUIRE_CUDA=1
            ;;
        --include-vulkan)
            REQUIRE_VULKAN=1
            ;;
        --include-all)
            INCLUDE_CPU=1
            REQUIRE_CUDA=1
            REQUIRE_VULKAN=1
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

if [[ "$AUTO_INCLUDE_GPU" == "0" && ( "$REQUIRE_CUDA" == "1" || "$REQUIRE_VULKAN" == "1" ) ]]; then
    echo "Cannot combine --cpu-only with GPU include flags." >&2
    exit 1
fi

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

for tool in dpkg-shlibdeps readelf pkg-config c++ getconf python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required packaging tool '$tool' was not found. Install dpkg-dev, binutils, build-essential, pkg-config, and python3." >&2
        exit 1
    fi
done

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

resolve_variant_inclusion() {
    local variant="$1"
    local required="$2"
    local src_dir="$PRECOMPILED_SRC/$variant"
    if [[ "$required" == "1" ]]; then
        if [[ ! -d "$src_dir" ]]; then
            echo "Requested precompiled variant '$variant' but '$src_dir' was not found." >&2
            exit 1
        fi
        echo "1"
        return 0
    fi
    if [[ "$AUTO_INCLUDE_GPU" == "1" && -d "$src_dir" ]]; then
        echo "1"
    else
        echo "0"
    fi
}

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

sanitize_precompiled_rpaths() {
    local root="$1"
    if [[ ! -d "$root" ]]; then
        return 0
    fi
    if ! command -v patchelf >/dev/null 2>&1; then
        echo "Warning: patchelf not found; packaging shared libraries without normalizing RUNPATHs." >&2
        return 0
    fi

    local lib=""
    while IFS= read -r -d '' lib; do
        patchelf --set-rpath '$ORIGIN' "$lib"
    done < <(find "$root" -regextype posix-extended -type f -regex '.*[.]so([.][0-9]+)*$' -print0)
}

INCLUDE_CUDA="$(resolve_variant_inclusion cuda "$REQUIRE_CUDA")"
INCLUDE_VULKAN="$(resolve_variant_inclusion vulkan "$REQUIRE_VULKAN")"

echo "Copying llama/ggml libraries"
if [[ -d "$PRECOMPILED_SRC" ]]; then
    mkdir -p "$PRECOMPILED_DST"
    # Keep root-level runtime payloads such as libpdfium.so.
    find "$PRECOMPILED_SRC" -mindepth 1 -maxdepth 1 \( -type f -o -type l \) \
        -exec cp -a {} "$PRECOMPILED_DST/" \;
    copy_variant_dir cpu "$INCLUDE_CPU"
    copy_variant_dir cuda "$INCLUDE_CUDA"
    copy_variant_dir vulkan "$INCLUDE_VULKAN"
    sanitize_precompiled_rpaths "$PRECOMPILED_DST"
else
    echo "Warning: '$PRECOMPILED_SRC' not found; packaging without bundled llama/ggml runtime libs." >&2
fi

SELECTED_VARIANTS=()
if [[ "$INCLUDE_CPU" == "1" ]]; then SELECTED_VARIANTS+=("cpu"); fi
if [[ "$INCLUDE_CUDA" == "1" ]]; then SELECTED_VARIANTS+=("cuda"); fi
if [[ "$INCLUDE_VULKAN" == "1" ]]; then SELECTED_VARIANTS+=("vulkan"); fi
echo "Included precompiled variants: ${SELECTED_VARIANTS[*]}"

SYSTEM_DEPENDS="$(python3 "$SCRIPT_DIR/deb_dependencies.py" "$PKG_ROOT" \
    --library-dir "$APP_DIR/include/external/llama.cpp/build-cuda-link" \
    --library-dir /usr/local/cuda/lib64 \
    --library-dir /opt/cuda/lib64)"
# Qt platform plugins and the WebP fallback decoder are loaded dynamically.
PACKAGE_DEPENDS_STR="$SYSTEM_DEPENDS, ca-certificates, qt6-wayland, libwebp7"

DESCRIPTION_TEXT="AI-powered file categorization tool. Requires the listed runtime libraries from the host system."
if [[ "$INCLUDE_VULKAN" == "1" ]]; then
    DESCRIPTION_TEXT+=" Includes the Vulkan backend and requires a working host Vulkan loader/driver stack."
fi
if [[ "$INCLUDE_CUDA" == "1" ]]; then
    DESCRIPTION_TEXT+=" CUDA-enabled builds require matching NVIDIA runtime libraries installed separately."
fi

if [[ -f "$REPO_ROOT/LICENSE" ]]; then
    install -m 0644 "$REPO_ROOT/LICENSE" "$PKG_ROOT/opt/aifilesorter/LICENSE"
fi

python3 "$SCRIPT_DIR/gen_run_wrapper.py" \
    --mode install \
    --install-app-dir "/opt/aifilesorter" \
    --binary "aifilesorter-bin" \
    --template "$SCRIPT_DIR/run_aifilesorter.sh.in" \
    --output "$PKG_ROOT/usr/bin/run_aifilesorter.sh"
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
