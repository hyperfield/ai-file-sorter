# AI File Sorter — Tests

> Parent: [../AGENTS.md](../AGENTS.md)

Catch2-based unit and integration tests for the AI File Sorter application. The suite is optional and enabled via `AI_FILE_SORTER_BUILD_TESTS=ON`.

## Structure

```
tests/
├── unit/                   # ~43 Catch2 test files covering core services and UI wiring
│   ├── TestHelpers.hpp     # Shared test utilities and fixtures
│   └── test_*.cpp          # One test module per production concern
└── helpers/                # Additional test support utilities
```

## Where to Look

| Task | File | Notes |
|------|------|-------|
| Categorization logic | `test_categorization_dialog.cpp` / `test_whitelist_and_prompt.cpp` | Prompt/whitelist behavior |
| LLM selection | `test_llm_selection_dialog_local.cpp` / `test_llm_selection_dialog_visual.cpp` | Local/visual model selection |
| Image analysis | `test_llava_image_analyzer.cpp` / `test_visual_llm_runtime.cpp` / `test_image_analyzer_factory.cpp` | Visual backend and image metadata |
| Media rename | `test_image_rename_metadata_service.cpp` / `test_media_rename_metadata_service.cpp` | EXIF/GPS and audio/video tags |
| Cache/learning | `test_cache_interactions.cpp` / `test_user_learning_store.cpp` / `test_database_manager_rename_only.cpp` | SQLite cache and learned behavior |
| Storage plugins | `test_storage_plugin_dialog.cpp` / `test_main_app_storage_support.cpp` | OneDrive/cloud compatibility |
| Updater | `test_updater.cpp` / `test_update_feed.cpp` / `test_updater_build_modes.cpp` | Update feed and installer logic |
| Utilities | `test_utils.cpp` / `test_ggml_runtime_paths.cpp` / `test_version.cpp` | Cross-platform helpers |
| Main window | `test_main_app_*.cpp` | UI state, language menus, image options, cache actions |

## Running Tests

CMake (preferred):
```bash
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON
cmake --build build-tests --target ai_file_sorter_tests --parallel $(nproc)
ctest --test-dir build-tests --output-on-failure -j $(nproc)
```

Convenience script:
```bash
cd app && ./scripts/rebuild_and_test.sh
```

## Conventions

- Tests are named `test_<production_class_or_feature>.cpp`.
- Use `TestHelpers.hpp` for common setup and fixture code.
- Production code exposes test hooks via `*TestAccess.hpp`/`cpp` shims rather than `friend` abuse.
- Each test file should compile independently; avoid global state.

## Notes

- The build directory `build-tests` must be unique per checkout path; do not reuse one from another clone.
- Some tests require the `libmediainfo` package to be available at configure time.
- On Windows, pass `-BuildTests` and `-RunTests` to `app/build_windows.ps1`.
- List individual cases: `./build-tests/ai_file_sorter_tests --list-tests`.
- Print all case names: `./build-tests/ai_file_sorter_tests --verbosity high --success`.