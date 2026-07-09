# Localized Quick Start Guide and FAQ Link

## Summary

Commit `f14ebb4` added a local Quick Start Guide to the Help menu and added a Help menu link to the website FAQ. The Quick Start body is bundled as local Markdown resources for every supported UI language.

## Motivation

The app already had website FAQ content, but users needed a lightweight local guide available from inside the app. The Help menu also needed direct access to the website FAQ for questions that are better maintained online.

## Implementation

`MainAppUiBuilder` now adds two Help menu actions:

- `Quick Start Guide`, which opens a local dialog
- `FAQ`, which opens `https://filesorter.app/faq/`

`MainAppHelpActions` loads the Quick Start Markdown resource that matches the current `TranslationManager` language. If a localized resource is unavailable, it falls back to English and then to a small built-in fallback string.

The Quick Start Markdown files are embedded through the app resources and are also added to the test resource target so unit tests can read them.

The normal Qt translation catalogs now include the Help menu labels and the Quick Start dialog title.

## Validation

The commit added or updated tests that verify:

- Quick Start Markdown content follows the selected app language
- Quick Start and FAQ labels translate across all supported UI languages
- `UiTranslator` includes the new actions

During implementation, the following targeted checks were run:

```bash
cmake --build build-tests --target ai_file_sorter_tests -j 4
./build-tests/ai_file_sorter_tests "Quick Start and FAQ help labels are translated for all supported UI languages"
./build-tests/ai_file_sorter_tests "Quick Start guide content follows the selected app language"
./build-tests/ai_file_sorter_tests "*UiTranslator*"
```

## User-visible impact

Users can open a local Quick Start Guide without leaving the app and can reach the online FAQ directly from Help. The guide and Help labels follow the selected UI language.

## Remaining caveats

The FAQ remains website-hosted, so it requires network access. The local Quick Start Guide is intentionally short and should stay focused on first-run workflow rather than becoming a full manual.
