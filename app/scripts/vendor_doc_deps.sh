#!/usr/bin/env bash
set -euo pipefail

LIBZIP_VERSION="1.11.4"
PUGIXML_VERSION="1.15"
PDFIUM_RELEASE="latest"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LIBZIP_DIR="$ROOT_DIR/external/libzip"
PUGIXML_DIR="$ROOT_DIR/external/pugixml"
PDFIUM_DIR="$ROOT_DIR/external/pdfium"
LICENSE_DIR="$ROOT_DIR/external/THIRD_PARTY_LICENSES"

mkdir -p "$LIBZIP_DIR" "$PUGIXML_DIR" "$LICENSE_DIR" \
  "$PDFIUM_DIR/linux-x64" "$PDFIUM_DIR/windows-x64" "$PDFIUM_DIR/macos-arm64" "$PDFIUM_DIR/macos-x64"

curl -L --fail "https://libzip.org/download/libzip-${LIBZIP_VERSION}.tar.xz" \
  -o "/tmp/libzip-${LIBZIP_VERSION}.tar.xz"
tar -xf "/tmp/libzip-${LIBZIP_VERSION}.tar.xz" --strip-components=1 -C "$LIBZIP_DIR"
if [ -f "$LIBZIP_DIR/LICENSE" ]; then
  cp "$LIBZIP_DIR/LICENSE" "$LICENSE_DIR/libzip-LICENSE"
fi

curl -L --fail "https://github.com/zeux/pugixml/releases/download/v${PUGIXML_VERSION}/pugixml-${PUGIXML_VERSION}.tar.gz" \
  -o "/tmp/pugixml-${PUGIXML_VERSION}.tar.gz"
tar -xf "/tmp/pugixml-${PUGIXML_VERSION}.tar.gz" --strip-components=1 -C "$PUGIXML_DIR"
if [ -f "$PUGIXML_DIR/LICENSE.md" ]; then
  cp "$PUGIXML_DIR/LICENSE.md" "$LICENSE_DIR/pugixml-LICENSE.md"
elif [ -f "$PUGIXML_DIR/LICENSE" ]; then
  cp "$PUGIXML_DIR/LICENSE" "$LICENSE_DIR/pugixml-LICENSE"
fi

curl -L --fail "https://github.com/bblanchon/pdfium-binaries/releases/${PDFIUM_RELEASE}/download/pdfium-linux-x64.tgz" \
  -o "/tmp/pdfium-linux-x64.tgz"
tar -xf "/tmp/pdfium-linux-x64.tgz" -C "$PDFIUM_DIR/linux-x64"
if [ -f "$PDFIUM_DIR/linux-x64/LICENSE" ]; then
  cp "$PDFIUM_DIR/linux-x64/LICENSE" "$LICENSE_DIR/pdfium-LICENSE"
elif [ -f "$PDFIUM_DIR/linux-x64/LICENSE.txt" ]; then
  cp "$PDFIUM_DIR/linux-x64/LICENSE.txt" "$LICENSE_DIR/pdfium-LICENSE.txt"
fi

curl -L --fail "https://github.com/bblanchon/pdfium-binaries/releases/${PDFIUM_RELEASE}/download/pdfium-win-x64.tgz" \
  -o "/tmp/pdfium-win-x64.tgz"
tar -xf "/tmp/pdfium-win-x64.tgz" -C "$PDFIUM_DIR/windows-x64"

curl -L --fail "https://github.com/bblanchon/pdfium-binaries/releases/${PDFIUM_RELEASE}/download/pdfium-mac-arm64.tgz" \
  -o "/tmp/pdfium-mac-arm64.tgz"
tar -xf "/tmp/pdfium-mac-arm64.tgz" -C "$PDFIUM_DIR/macos-arm64"

PDFIUM_MAC_X64_TGZ="${PDFIUM_MAC_X64_TGZ:-pdfium-mac-x64.tgz}"
curl -L --fail "https://github.com/bblanchon/pdfium-binaries/releases/${PDFIUM_RELEASE}/download/${PDFIUM_MAC_X64_TGZ}" \
  -o "/tmp/${PDFIUM_MAC_X64_TGZ}"
tar -xf "/tmp/${PDFIUM_MAC_X64_TGZ}" -C "$PDFIUM_DIR/macos-x64"

cat > "$PDFIUM_DIR/README.md" <<'DOC'
# PDFium prebuilts

This folder is populated by `app/scripts/vendor_doc_deps.sh` or `app/scripts/vendor_doc_deps.ps1`.
Expected layout:

- linux-x64/
- windows-x64/
- macos-arm64/
- macos-x64/

Each folder should contain `include/` and the platform PDFium library under `lib/`:

- Linux: `lib/libpdfium.so`
- Windows: `bin/pdfium.dll` + `lib/pdfium.dll.lib`
- macOS: `lib/libpdfium.dylib` (arm64 or x64)
DOC

printf "Done. You can now commit external/libzip, external/pugixml, and external/pdfium.\n"
