#!/usr/bin/env bash

set -euo pipefail

# Builds a Debian package for AI File Sorter that bundles only the project-specific
# llama/ggml libraries and assumes all other runtime libraries are supplied by the system.
#
# Usage:
#   ./package_deb.sh [version]
# If no version is supplied, the script reads app/include/app_version.hpp.

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
APP_DIR="$REPO_ROOT/app"

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

VERSION="${1:-$(VERSION_FROM_HEADER "$APP_DIR/include/app_version.hpp")}"

if [[ -z "$VERSION" ]]; then
    echo "Failed to determine package version." >&2
    exit 1
fi

BIN_PATH="$APP_DIR/bin/aifilesorter"
if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary not found at $BIN_PATH — running make." >&2
    make -C "$APP_DIR"
fi

if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary still missing after build attempt." >&2
    exit 1
fi

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

echo "Copying llama/ggml libraries"
if [[ -d "$APP_DIR/lib/precompiled" ]]; then
    cp -a "$APP_DIR/lib/precompiled" "$PKG_ROOT/opt/aifilesorter/lib/"
fi

if [[ -f "$APP_DIR/resources/certs/cacert.pem" ]]; then
    install -m 0644 "$APP_DIR/resources/certs/cacert.pem" "$PKG_ROOT/opt/aifilesorter/certs/cacert.pem"
fi

if [[ -f "$REPO_ROOT/LICENSE" ]]; then
    install -m 0644 "$REPO_ROOT/LICENSE" "$PKG_ROOT/opt/aifilesorter/LICENSE"
fi

cat > "$PKG_ROOT/usr/bin/aifilesorter" <<'EOF'
#!/bin/sh
APP_DIR="/opt/aifilesorter"
CPU_LIB_DIR="$APP_DIR/lib/precompiled/cpu/bin"
CUDA_LIB_DIR="$APP_DIR/lib/precompiled/cuda/bin"
PLATFORM_CANDIDATES="/usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/qt6/plugins /lib/x86_64-linux-gnu/qt6/plugins"

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

SELECTED_LIB_DIR="$(choose_cuda_path)"
PATH_COMPONENTS="$CPU_LIB_DIR"
if [ -n "$SELECTED_LIB_DIR" ] && [ -d "$SELECTED_LIB_DIR" ] && [ "$SELECTED_LIB_DIR" != "$CPU_LIB_DIR" ]; then
    PATH_COMPONENTS="$SELECTED_LIB_DIR:$PATH_COMPONENTS"
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
chmod 0755 "$PKG_ROOT/usr/bin/aifilesorter"

CONTROL_FILE="$PKG_ROOT/DEBIAN/control"
cat > "$CONTROL_FILE" <<EOF
Package: aifilesorter
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: AI File Sorter Team <support@example.com>
Installed-Size: 0
Depends: libc6 (>= 2.31), libstdc++6 (>= 12), libgcc-s1 (>= 12), libqt6widgets6 (>= 6.2), libqt6gui6 (>= 6.2), libqt6core6 (>= 6.2), libqt6dbus6 (>= 6.2), qt6-wayland, libcurl4, libjsoncpp25, libsqlite3-0, libfmt8, libssl3, libopenblas0-pthread
Description: AI File Sorter desktop application
 AI-powered file categorization tool. Requires the listed runtime libraries from the host system. GPU acceleration needs NVIDIA CUDA libraries installed separately.
EOF
chmod 0644 "$CONTROL_FILE"

echo "Adjusting permissions"
find "$PKG_ROOT" -type d -exec chmod 755 {} +
find "$PKG_ROOT/opt/aifilesorter/lib" -type f -exec chmod 0644 {} +
chmod 0755 "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter-bin"
chmod 0755 "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter"
chmod 0755 "$PKG_ROOT/usr/bin/aifilesorter"

SIZE_KB=$(du -sk "$PKG_ROOT" | cut -f1)
sed -i "s/^Installed-Size: .*/Installed-Size: ${SIZE_KB}/" "$CONTROL_FILE"

mkdir -p "$OUT_DIR"
DEB_PATH="$OUT_DIR/${PKG_NAME}_amd64.deb"
rm -f "$DEB_PATH"

echo "Building package $DEB_PATH"
dpkg-deb --build "$PKG_ROOT" "$OUT_DIR"

echo "Done. Package created at $DEB_PATH"
