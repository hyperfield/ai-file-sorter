# Changelog

## [1.5.0] - 2025-12-25

- **Added Google Gemini API support** with smart free-tier optimization
  - Intelligent rate limiting (15 RPM) prevents quota exhaustion
  - Adaptive timeout handling (20-240s) eliminates timeout errors for free users
  - Self-operating retry logic with exponential backoff
  - Persistent state tracking for optimal request pacing
- Complete UI integration for Gemini API key management in Select LLM dialog
- Fixed LLMClient implementation to properly match header interface
- Enhanced error handling and credential validation for both OpenAI and Gemini
- Updated settings to support separate API keys and models for Gemini

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




