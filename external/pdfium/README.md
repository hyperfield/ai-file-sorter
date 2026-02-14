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
