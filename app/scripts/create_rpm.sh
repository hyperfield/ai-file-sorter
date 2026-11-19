#!/usr/bin/env bash

set -euo pipefail

# Builds an RPM package for AI File Sorter that bundles the project-specific
# llama/ggml runtime payloads under /opt/aifilesorter and assumes the rest of
# the desktop/runtime stack is supplied by the host system.
#
# Usage:
#   ./create_rpm.sh [options] [version]
# If no version is supplied, the script reads app/include/app_version.hpp.
# CPU runtime is always included. Staged CUDA/Vulkan variants are auto-included
# by default unless --cpu-only is passed.

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
APP_DIR="$REPO_ROOT/app"

usage() {
    cat <<'EOF'
Usage: ./create_rpm.sh [options] [version]

Options:
  --cpu-only         Include only the CPU runtime, even if GPU variants are staged.
  --include-cuda     Include precompiled CUDA runtime libs (app/lib/precompiled/cuda)
  --include-vulkan   Include precompiled Vulkan runtime libs (app/lib/precompiled/vulkan)
  --include-all      Include CPU + CUDA + Vulkan precompiled runtime libs
  -h, --help         Show this help

Environment:
  RPM_RELEASE        RPM release number (default: 1)

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

join_by_space() {
    local first=1
    local item
    for item in "$@"; do
        if [[ "$first" == "1" ]]; then
            printf '%s' "$item"
            first=0
        else
            printf ' %s' "$item"
        fi
    done
    printf '\n'
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

VERSION="${VERSION_ARG:-$(version_from_header "$APP_DIR/include/app_version.hpp")}"
RPM_VERSION="${VERSION//-/_}"
RPM_RELEASE="${RPM_RELEASE:-1}"
RPM_ARCH="${RPM_ARCH:-$(rpm --eval '%{_arch}')}"

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

OUT_DIR="$REPO_ROOT/dist/aifilesorter_rpm"
RPMBUILD_TOPDIR="$OUT_DIR/rpmbuild"
PAYLOAD_NAME="aifilesorter-${RPM_VERSION}"
PAYLOAD_PARENT="$OUT_DIR/payload"
PAYLOAD_ROOT="$PAYLOAD_PARENT/$PAYLOAD_NAME"

PRECOMPILED_SRC="$APP_DIR/lib/precompiled"
PRECOMPILED_DST="$PAYLOAD_ROOT/opt/aifilesorter/lib/precompiled"

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

echo "Staging RPM payload in $PAYLOAD_ROOT"
rm -rf "$PAYLOAD_ROOT" "$RPMBUILD_TOPDIR"
mkdir -p \
    "$PAYLOAD_ROOT/opt/aifilesorter/bin" \
    "$PAYLOAD_ROOT/opt/aifilesorter/lib" \
    "$PAYLOAD_ROOT/opt/aifilesorter/certs" \
    "$PAYLOAD_ROOT/usr/bin" \
    "$RPMBUILD_TOPDIR/BUILD" \
    "$RPMBUILD_TOPDIR/BUILDROOT" \
    "$RPMBUILD_TOPDIR/RPMS" \
    "$RPMBUILD_TOPDIR/SOURCES" \
    "$RPMBUILD_TOPDIR/SPECS" \
    "$RPMBUILD_TOPDIR/SRPMS"

install -m 0755 "$BIN_PATH" "$PAYLOAD_ROOT/opt/aifilesorter/bin/aifilesorter-bin"
ln -sf aifilesorter-bin "$PAYLOAD_ROOT/opt/aifilesorter/bin/aifilesorter"

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

if [[ -f "$REPO_ROOT/LICENSE" ]]; then
    install -m 0644 "$REPO_ROOT/LICENSE" "$PAYLOAD_ROOT/opt/aifilesorter/LICENSE"
fi

python3 "$SCRIPT_DIR/gen_run_wrapper.py" \
    --mode install \
    --install-app-dir "/opt/aifilesorter" \
    --binary "aifilesorter-bin" \
    --template "$SCRIPT_DIR/run_aifilesorter.sh.in" \
    --output "$PAYLOAD_ROOT/usr/bin/run_aifilesorter.sh"
chmod 0755 "$PAYLOAD_ROOT/usr/bin/run_aifilesorter.sh"
ln -sf run_aifilesorter.sh "$PAYLOAD_ROOT/usr/bin/aifilesorter"

FILELIST="$RPMBUILD_TOPDIR/SOURCES/${PAYLOAD_NAME}.files"
{
    if [[ -f "$PAYLOAD_ROOT/opt/aifilesorter/LICENSE" ]]; then
        echo "%license /opt/aifilesorter/LICENSE"
    fi
    while IFS= read -r -d '' file_path; do
        local_path="/${file_path#"$PAYLOAD_ROOT"/}"
        if [[ "$local_path" == "/opt/aifilesorter/LICENSE" ]]; then
            continue
        fi
        echo "$local_path"
    done < <(find "$PAYLOAD_ROOT" \( -type f -o -type l \) -print0 | sort -z)
} > "$FILELIST"

TARBALL="$RPMBUILD_TOPDIR/SOURCES/${PAYLOAD_NAME}.tar.gz"
tar -C "$PAYLOAD_PARENT" -czf "$TARBALL" "$PAYLOAD_NAME"

DESCRIPTION_TEXT="AI-powered file categorization tool. Includes the CPU runtime by default."
if [[ "$INCLUDE_VULKAN" == "1" ]]; then
    DESCRIPTION_TEXT+=" Vulkan runtime payloads are bundled and are used when the host Vulkan stack is available."
fi
if [[ "$INCLUDE_CUDA" == "1" ]]; then
    DESCRIPTION_TEXT+=" CUDA runtime payloads are bundled and are used when matching NVIDIA runtime libraries are available."
fi
DESCRIPTION_TEXT+=" The launcher falls back to CPU automatically when GPU backends are unavailable."

# Private libraries do not advertise global Provides, so their SONAME requirements
# must also be excluded. Ordinary host-library requirements remain generated.
BUNDLED_REQUIRES_EXCLUDE="$(python3 "$SCRIPT_DIR/rpm_dependencies.py" "$PRECOMPILED_DST")"
OPTIONAL_REQUIRES_EXCLUDE=""
if [[ "$INCLUDE_CUDA" == "1" && "$INCLUDE_VULKAN" == "1" ]]; then
    OPTIONAL_REQUIRES_EXCLUDE="%global __requires_exclude_from ^/opt/aifilesorter/lib/precompiled/(cuda|vulkan)(/.*)?$"
elif [[ "$INCLUDE_CUDA" == "1" ]]; then
    OPTIONAL_REQUIRES_EXCLUDE="%global __requires_exclude_from ^/opt/aifilesorter/lib/precompiled/cuda(/.*)?$"
elif [[ "$INCLUDE_VULKAN" == "1" ]]; then
    OPTIONAL_REQUIRES_EXCLUDE="%global __requires_exclude_from ^/opt/aifilesorter/lib/precompiled/vulkan(/.*)?$"
fi

SPEC_FILE="$RPMBUILD_TOPDIR/SPECS/aifilesorter.spec"
CHANGELOG_DATE="$(LC_ALL=C date '+%a %b %d %Y')"
cat > "$SPEC_FILE" <<EOF
%global debug_package %{nil}
%global _build_id_links none
%global __provides_exclude_from ^/opt/aifilesorter/lib/precompiled/.*$
${BUNDLED_REQUIRES_EXCLUDE}
${OPTIONAL_REQUIRES_EXCLUDE}
Name:           aifilesorter
Version:        ${RPM_VERSION}
Release:        ${RPM_RELEASE}%{?dist}
Summary:        AI File Sorter desktop application
License:        AGPL-3.0-only
URL:            https://github.com/hyperfield/ai-file-sorter
BuildArch:      ${RPM_ARCH}
Requires:       ca-certificates
Source0:        ${PAYLOAD_NAME}.tar.gz
Source1:        ${PAYLOAD_NAME}.files

%description
${DESCRIPTION_TEXT}

%prep
%setup -q -n ${PAYLOAD_NAME}

%build
# Prebuilt payload; nothing to compile during rpmbuild.

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a opt %{buildroot}/
cp -a usr %{buildroot}/

%files -f %{SOURCE1}

%changelog
* ${CHANGELOG_DATE} AI File Sorter Team <support@example.com> - ${RPM_VERSION}-${RPM_RELEASE}
- Automated RPM packaging build
EOF

echo "Building RPM via rpmbuild"
rpmbuild \
    --define "_topdir ${RPMBUILD_TOPDIR}" \
    -bb "$SPEC_FILE"

mapfile -t built_rpms < <(find "$RPMBUILD_TOPDIR/RPMS" -type f -name '*.rpm' | sort)
if [[ ${#built_rpms[@]} -eq 0 ]]; then
    echo "rpmbuild completed but no RPM artifacts were found." >&2
    exit 1
fi

FINAL_DIR="$OUT_DIR/final"
mkdir -p "$FINAL_DIR"
for rpm_path in "${built_rpms[@]}"; do
    cp -f "$rpm_path" "$FINAL_DIR/"
done

echo "Done. RPM package(s) created:"
join_by_space "${built_rpms[@]}" | tr ' ' '\n'
echo "Convenience copies:"
join_by_space "$FINAL_DIR"/*.rpm | tr ' ' '\n'
