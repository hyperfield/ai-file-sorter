# Vendored document-analysis dependencies

This repo vendors the following dependencies for embedded document extraction:

- libzip (ZIP container access)
- pugixml (XML parsing)
- PDFium (PDF text extraction)

Use `app/scripts/vendor_doc_deps.sh` (or `app/scripts/vendor_doc_deps.ps1` on Windows) to download and populate `external/`.
The script also stages third-party license files under `external/THIRD_PARTY_LICENSES`.

Commit the populated directories for release builds (Linux/Windows/macOS ARM).
