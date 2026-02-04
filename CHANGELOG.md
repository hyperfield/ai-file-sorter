# Changelog

## [1.6.0] - 2026-02-01

- Added document content analysis (text LLM) with rename-only/document-only options and optional creation-date suffixes for categories. Supported document formats include PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
- Local 3B model download now defaults to Q4 for better GPU compatibility. The legacy Local 3B Q8 is still selectable when an existing download is found.
- Improved the LLM selection dialog latency.
- Added custom API endpoints to the Select LLM dialog. Custom endpoints accept base URLs or full /chat/completions endpoints, with optional API keys for local servers.
- LLM-derived categorizations and rename suggestions are now saved as you go, so progress isn't lost if the app closes unexpectedly.
- Image analysis now falls back (with a user prompt) to CPU if the GPU has insufficient available memory.
- Review dialog now lets you select highlighted rows and bulk edit their categories.
- Review dialog is now scrollable on smaller screens so action buttons stay visible.
- Improved subcategory consistency by merging labels that only differ by generic suffixes (e.g., “files”).
- Added a system compatibility check (benchmarking) to determine the most suitable LLM for your system.
- Added Korean as an interface language.
- macOS builds now include variant `make` targets for Apple Silicon (M1 / M2-M3) and Intel outputs, plus improved arch-aware llama.cpp builds.
- UI and usability improvements.

## [1.5.0] - 2026-01-11

- Added content analysis for picture files via LLaVA (visual LLM), with separate model + mmproj downloads in the Select LLM dialog.
- Added image analysis options in the main window (analyze images, offer rename suggestions, rename-only mode).
- Added an image-only processing toggle to focus runs on supported picture files and disable standard categorization controls.
- Added document content analysis (text LLM) with rename-only/document-only modes and optional creation-date suffixes for categories.
- Added support for document formats including PDF, DOCX, XLSX, PPTX, ODT, ODS, and ODP (plus common text formats).
- Document analysis now uses embedded PDFium/libzip/pugixml in bundled builds (no pdftotext/unzip requirement).
- Review dialog now supports rename-only flows, suggested filename edits, and status labels for Renamed / Renamed & Moved.
- Track applied picture renames so already-renamed files are not reprocessed; rename-only review hides them while categorization review keeps them visible for folder moves.
- Added Dutch as a selectable interface language.
- Analysis progress dialog output is now fully localized (status tags, scan/process lines, and file/directory labels) to match the selected UI language.
- Build/test updates: mtmd progress callback auto-detection, mtmd-cli build fix, and new Catch2 tests for rename-only caching.

## [1.4.5] - 2025-12-05

- Added support for Gemini (a remote LLM) - with your own Gemini API key.
- Fixed compile under Arch Linux.

## [1.4.0] - 2025-12-05

- Added dry run / preview-only mode with From/To table, no moves performed until you uncheck.
- Persistent Undo: the latest sort saves a plan file; use Edit -> "Undo last run" even after closing dialogs.
- UI tweaks: Name column auto-resizes, new translations for dry run/undo strings, Undo moved to top of Edit menu.
- A few more guard rails added.
- Remote LLM flow now uses your own OpenAI API key (any ChatGPT model supported); the bundled remote key and obfuscation step were removed.

## [1.3.0] - 2025-11-21

- You can now switch between two categorization modes: More Refined and More Consistent. Choose depending on your folder and use case.
- Added optional Whitelists - limit the number and names of categories when needed.
- Added sorting by file names, categories, subcategories in the Categorization Review dialog.
- You can now add a custom Local LLM in the Select LLM dialog.
- Multilingual categorization: the file categorization labels can now be assigned in Dutch, French, German, Italian, Polish, Portuguese, Spanish, and Turkish.
- New interface languages: Dutch, German, Italian, Polish, Portugese, Spanish, and Turkish.

## [1.1.0] - 2025-11-08

- New feature: Support for Vulkan. This means that many non-Nvidia graphics cards (GPUs) are now supported for compute acceleration during local LLM inference.
- New feature: Toggle subcategories in the categorization review dialog.
- New feature: Undo the recent file sort (move) action.
- Fixes: Bug fixes and stability improvements.
- Added a CTest-integrated test suite. Expanded test coverage.
- Code optimization refactors.

## [1.0.0] - 2025-10-30

- Migrated the entire desktop UI from GTK/Glade to a native Qt6 interface.
- Added selection boxes for files in the categorization review dialog.
- Added internatioinalization framework and the French translation for the user interface.
- Added refreshed menu icons, mnemonic behaviour, and persistent File Explorer settings.
- Simplified cross-platform builds (Linux/macOS) around Qt6; retired the MSYS2/GTK toolchain.
- Optimized and cleaned up the code. Fixed error-prone areas.
- Modernized the build pipeline. Introduced CMake for compilation on Windows.

## [0.9.7] - 2025-10-19

- Added paths to files in LLM requests for more context.
- Added taxonomy for more consistent assignment of categories across categorizations.
  (Narrowing down the number of categories and subcategories).
- Improved the readability of the categorization progress dialog box.
- Improved the stability of CUDA detection and interaction.
- Added more logging coverage throughout the code base.

## [0.9.3] - 2025-09-22

- Added compatibility with CUDA 13.

## [0.9.2] - 2025-08-06

- Bug fixes.
- Increased code coverage with logging.

## [0.9.1] - 2025-08-01

- Bug fixes.
- Minor improvements for stability.
- Removed the deprecated GPU backend from the runtime build.

## [0.9.0] - 2025-07-18

- Local LLM support with `llama.cpp`.
- LLM selection and download dialog.
- Improved `Makefile` for a more hassle-free build and installation.
- Minor bug fixes and improvements.
