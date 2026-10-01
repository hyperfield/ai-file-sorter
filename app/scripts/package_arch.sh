#!/usr/bin/env bash

set -euo pipefail

# Builds an Arch Linux pacman package for AI File Sorter that bundles the
# project-specific llama/ggml runtime payloads under /opt/aifilesorter and
# assumes the remaining desktop/runtime libraries are supplied by the host.
#
# Usage:
#   ./package_arch.sh [options] [version]
# If no version is supplied, the script reads app/include/app_version.hpp.

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
APP_DIR="$REPO_ROOT/app"

usage() {
    cat <<'EOF'
Usage: ./package_arch.sh [options] [version]

Options:
  --cpu-only         Include only the CPU runtime, even if GPU variants are staged.
  --include-cuda     Include precompiled CUDA runtime libs (app/lib/precompiled/cuda)
  --include-vulkan   Include precompiled Vulkan runtime libs (app/lib/precompiled/vulkan)
  --include-all      Include CPU + CUDA + Vulkan precompiled runtime libs
  -h, --help         Show this help

Environment:
  PKGREL             Arch package release number (default: 1)
  PKG_ARCH           Arch package architecture (default: x86_64)

Notes:
  - CPU precompiled libs are included by default.
  - Staged CUDA/Vulkan runtime dirs are auto-included when present unless --cpu-only is used.
  - Root files in app/lib/precompiled (e.g. libpdfium.so) are always included when present.
EOF
}

version_from_header() {
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

if [[ "$AUTO_INCLUDE_GPU" == "0" && ( "$REQUIRE_CUDA" == "1" || "$REQUIRE_VULKAN" == "1" ) ]]; then
    echo "Cannot combine --cpu-only with GPU include flags." >&2
    exit 1
fi

for tool in bsdtar gzip python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required packaging tool '$tool' was not found." >&2
        exit 1
    fi
done

VERSION="${VERSION_ARG:-$(version_from_header "$APP_DIR/include/app_version.hpp")}"
PKG_VERSION="${VERSION//-/_}"
PKG_REL="${PKGREL:-1}"
PKG_ARCH="${PKG_ARCH:-x86_64}"
BUILD_DATE="${SOURCE_DATE_EPOCH:-$(date +%s)}"

if [[ -z "$PKG_VERSION" ]]; then
    echo "Failed to determine package version." >&2
    exit 1
fi

BIN_PATH="$APP_DIR/bin/aifilesorter-bin"
if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary not found at $BIN_PATH - running make." >&2
    make -C "$APP_DIR"
fi

if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary still missing after build attempt." >&2
    exit 1
fi

OUT_DIR="$REPO_ROOT/dist/aifilesorter_arch"
PKG_ROOT="$OUT_DIR/pkg/aifilesorter"
FINAL_DIR="$OUT_DIR/final"
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

echo "Staging Arch package payload in $PKG_ROOT"
rm -rf "$PKG_ROOT"
mkdir -p \
    "$PKG_ROOT/opt/aifilesorter/bin" \
    "$PKG_ROOT/opt/aifilesorter/lib" \
    "$PKG_ROOT/opt/aifilesorter/certs" \
    "$PKG_ROOT/usr/bin"

install -m 0755 "$BIN_PATH" "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter-bin"
ln -sf aifilesorter-bin "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter"

echo "Copying llama/ggml libraries"
if [[ -d "$PRECOMPILED_SRC" ]]; then
    mkdir -p "$PRECOMPILED_DST"
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

if [[ -f "$APP_DIR/resources/certs/cacert.pem" ]]; then
    install -m 0644 "$APP_DIR/resources/certs/cacert.pem" "$PKG_ROOT/opt/aifilesorter/certs/cacert.pem"
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

PACKAGE_DEPENDS=(
    "glibc"
    "gcc-libs"
    "qt6-base"
    "curl"
    "jsoncpp"
    "sqlite"
    "openssl"
    "fmt"
    "spdlog"
    "libmediainfo"
    "openblas"
    "zlib"
)

PACKAGE_OPTDEPENDS=(
    "qt6-wayland: native Wayland Qt platform integration"
)
if [[ "$INCLUDE_VULKAN" == "1" ]]; then
    PACKAGE_OPTDEPENDS+=("vulkan-icd-loader: Vulkan backend runtime support")
fi
if [[ "$INCLUDE_CUDA" == "1" ]]; then
    PACKAGE_OPTDEPENDS+=("cuda: CUDA backend runtime support")
    PACKAGE_OPTDEPENDS+=("nvidia-utils: NVIDIA driver/runtime support")
fi

PACKAGE_SIZE="$(du -sb "$PKG_ROOT/opt" "$PKG_ROOT/usr" | awk '{ total += $1 } END { print total + 0 }')"
PKGINFO="$PKG_ROOT/.PKGINFO"
{
    echo "# Generated by AI File Sorter package_arch.sh"
    echo "pkgname = aifilesorter"
    echo "pkgbase = aifilesorter"
    echo "xdata = pkgtype=pkg"
    echo "pkgver = ${PKG_VERSION}-${PKG_REL}"
    echo "pkgdesc = AI File Sorter desktop application"
    echo "url = https://github.com/hyperfield/ai-file-sorter"
    echo "builddate = ${BUILD_DATE}"
    echo "packager = AI File Sorter Team <support@example.com>"
    echo "size = ${PACKAGE_SIZE}"
    echo "arch = ${PKG_ARCH}"
    echo "license = AGPL-3.0-only"
    for dep in "${PACKAGE_DEPENDS[@]}"; do
        echo "depend = ${dep}"
    done
    for optdep in "${PACKAGE_OPTDEPENDS[@]}"; do
        echo "optdepend = ${optdep}"
    done
} > "$PKGINFO"
chmod 0644 "$PKGINFO"

(
    cd "$PKG_ROOT"
    bsdtar \
        -cf - \
        --format=mtree \
        --options='!all,use-set,type,uid,gid,mode,time,size,md5,sha256,link' \
        --exclude .PKGINFO \
        --exclude .MTREE \
        . | gzip -c -n > .MTREE
)
chmod 0644 "$PKG_ROOT/.MTREE"

mkdir -p "$FINAL_DIR"
PKG_PATH="$FINAL_DIR/aifilesorter-${PKG_VERSION}-${PKG_REL}-${PKG_ARCH}.pkg.tar.zst"
rm -f "$PKG_PATH"

echo "Building Arch package $PKG_PATH"
(
    cd "$PKG_ROOT"
    bsdtar \
        --uid 0 \
        --gid 0 \
        --uname root \
        --gname root \
        --numeric-owner \
        --zstd \
        -cf "$PKG_PATH" \
        .PKGINFO \
        .MTREE \
        opt \
        usr
)

echo "Done. Arch package created at $PKG_PATH"
