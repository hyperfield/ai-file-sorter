# AI File Sorter

> Parent: [~/AGENTS.md](../../AGENTS.md) — environment-wide · [~/Projects/AGENTS.md](../AGENTS.md) — project index

Cross-platform desktop app that uses local or remote LLMs to categorize files, suggest names for images/documents/media, and organize folders. Local-first, privacy-oriented, AGPL licensed.

## Stack

- C++20 with Qt6 UI (Widgets/Gui/Core/Network)
- llama.cpp (submodule under `app/include/external/llama.cpp`) for local inference
- CMake + platform-specific Makefiles for builds
- Vendored: PDFium, libzip, pugixml; external: Catch2 for tests
- Package-managed only: `libmediainfo` (vendored copies are rejected by the build)

## Structure

```
.
├── app/                  # Main application
│   ├── main.cpp          # QApplication entry point, CLI parsing, test hooks
│   ├── lib/*.cpp         # Core implementation (~80 modules)
│   ├── include/*.hpp     # Headers, organized next to implementation
│   ├── include/llama/    # Minimal llama.cpp wrapper header
│   ├── include/external/ # Vendored llama.cpp submodule (read-only conventions)
│   ├── CMakeLists.txt    # Primary CMake build (Windows, CI, tests)
│   ├── Makefile          # Linux/macOS convenience build
│   ├── scripts/          # Build/packaging/diagnostic helpers
│   └── resources/        # Qt resources, translations, icons, help
├── external/             # Vendored dependencies (libzip, pugixml, pdfium, Catch2)
├── scripts/              # Git hooks and repo-wide helpers
├── tests/unit/           # Catch2 unit tests (~43 files)
├── README.md             # End-user documentation, installation, env vars
├── TROUBLESHOOTING.md    # Log locations and diagnostics
└── TESTS.md              # Testing notes
```

## Where to Look

| Task | Location | Notes |
|------|----------|-------|
| Main window / UI flow | `app/lib/MainApp.cpp` + `app/include/MainApp.hpp` | Central Qt main window; builds menus, binds state, drives analysis |
| File scanning | `app/lib/FileScanner.cpp` + `app/include/FileScanner.hpp` | Walks target directories, produces file list |
| Categorization orchestration | `app/lib/CategorizationService.cpp` / `AnalysisCoordinator.cpp` / `AnalysisEntryRouter.cpp` | Routes files to text/visual/media analyzers, applies LLM |
| Local LLM backend | `app/lib/LocalLLMClient.cpp` / `VisualLlmRuntime.cpp` / `LlavaImageAnalyzer.cpp` | llama.cpp wrapper, visual backend runtime, image analysis |
| Remote LLM backends | `app/lib/LLMClient.cpp` / `GeminiClient.cpp` / `CustomApiDialog.cpp` | OpenAI-compatible, Gemini, custom endpoints |
| Document text extraction | `app/lib/DocumentTextAnalyzer.cpp` / `PugixmlBundle.cpp` | PDFium, libzip, pugixml-based extraction |
| Media metadata renaming | `app/lib/MediaRenameMetadataService.cpp` / `ImageRenameMetadataService.cpp` | ID3/Vorbis/MP4 atom and image EXIF/GPS metadata |
| Categorization review | `app/lib/CategorizationDialog.cpp` / `CategorizationProgressDialog.cpp` / `ResultsCoordinator.cpp` | Review dialogs, dry-run preview, confirmation |
| Settings & persistence | `app/lib/Settings.cpp` / `DatabaseManager.cpp` / `UserLearningStore.cpp` | `config.ini`, SQLite cache, learned approvals |
| Undo | `app/lib/UndoManager.cpp` | Persistent undo of last sort run |
| Updater | `app/lib/Updater.cpp` / `UpdateFeed.cpp` / `UpdateInstaller.cpp` | Platform-specific update modes compiled in |
| Translations | `app/lib/TranslationManager.cpp` / `UiTranslator.cpp` | 15+ locales via Qt translations |
| Storage plugins | `app/lib/StoragePluginManager.cpp` / `OneDriveStorageProvider.cpp` | Cloud-folder compatibility plugins |
| Tests | `tests/unit/*.cpp` | Catch2 tests; run via CMake or `app/scripts/rebuild_and_test.sh` |
| Build helpers | `app/scripts/build_llama_*.sh` / `package_deb.sh` / `create_rpm.sh` / `build_windows.ps1` | Build llama variants and package per platform |

## Build Commands

Linux/macOS (source):
```bash
cd app
make -j4
# binary: app/bin/aifilesorter (Linux wrapper: app/bin/run_aifilesorter.sh)
```

CMake (Windows, tests, CI):
```bash
cmake -S app -B build -DAI_FILE_SORTER_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Windows bundled build:
```powershell
app\build_windows.ps1 -Configuration Release
```

## Conventions

- Code lives in `app/lib/*.cpp` with matching `app/include/*.hpp`.
- Test access shims are named `*TestAccess.cpp`/`hpp` and live alongside production code.
- Qt UI files, translations, and resources are under `app/resources/`.
- Platform wrappers: `app/startapp_linux.cpp` and `app/startapp_windows.cpp` for launcher logic.
- Use `app/Makefile` for day-to-day Linux/macOS development; `app/CMakeLists.txt` is the canonical cross-platform build.

## Anti-Patterns (This Project)

- **Do not vendor MediaInfo**: `libmediainfo` must come from the system package manager (apt/dnf/brew/vcpkg). The Makefile explicitly errors on vendored copies.
- **Do not silently drop PDFium**: Embedded PDF extraction is required by default (`AI_FILE_SORTER_REQUIRE_EMBEDDED_PDF_BACKEND=ON`). Opt out only intentionally.
- **Do not mix llama.cpp build variants blindly**: CUDA and Vulkan are built separately; the runtime launcher picks the best available backend.
- **Do not edit `app/include/external/llama.cpp/` directly**: It is a tracked submodule.
- **Do not reuse `build-tests` across checkout paths**: CMake records the source path; use a fresh build directory per checkout.

## Commands for Reference

```bash
# Linux/macOS full dev build
cd app && make -j4

# Run tests (requires Catch2 submodule)
cd app && ./scripts/rebuild_and_test.sh

# Build a specific backend variant (Linux)
./app/scripts/build_llama_linux.sh cuda=off vulkan=off
./app/scripts/build_llama_linux.sh cuda=on vulkan=off
./app/scripts/build_llama_linux.sh cuda=off vulkan=on

# Debian/RPM packages
./app/scripts/package_deb.sh --cpu-only | --include-vulkan | --include-cuda
./app/scripts/create_rpm.sh --cpu-only | --include-vulkan | --include-cuda

# Diagnostics (platform-specific)
./app/scripts/collect_linux_diagnostics.sh
./app/scripts/collect_macos_diagnostics.sh
.\app\scripts\collect_windows_diagnostics.ps1
```

## Notes

- This repo is **not** on GitHub; development is local/private.
- No CI/CD or git hooks are configured by default; see `scripts/hooks/` if you want to add them.
- The project embeds large vendored binaries (`app/lib/precompiled/`, `external/pdfium/`). Cloning requires `git submodule update --init --recursive`.
- Environment variables are documented extensively in `README.md` (GPU backend selection, timeouts, updater feeds, config/cache paths).
