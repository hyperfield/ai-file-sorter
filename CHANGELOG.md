# Changelog

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
