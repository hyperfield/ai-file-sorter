# Hindi UI Support and Translation Coverage

## Summary

Commit `a18f76b` adds Hindi as a supported interface language and expands translation coverage so both top-level menus and visible menu actions are tracked and tested across all shipped UI languages.

This is more than a new `.ts` file. It also fixes translation discoverability for several menu labels that had not been reliably exposed to `lupdate`.

## Motivation

The app already shipped multiple UI languages, but Hindi was missing from the interface-language list. At the same time, some menu labels were easy to miss in translation maintenance because they were not consistently expressed as explicit translatable keys in the UI translation layer.

That combination meant it was possible to add a language and still ship English fallbacks inside visible menus.

## Implementation

Hindi is now part of the language enum, the translation manager catalog, the UI action wiring, and the bundled resources:

```cpp
enum class Language {
    English,
    French,
    German,
    Hindi,
    Italian,
    Spanish,
    Turkish,
    Korean,
    Dutch
};
```

The UI translator also now declares menu titles and action labels as explicit `QT_TRANSLATE_NOOP` keys:

```cpp
constexpr auto kMenuTitleEdit = QT_TRANSLATE_NOOP("UiTranslator", "&Edit");
constexpr auto kMenuTitleView = QT_TRANSLATE_NOOP("UiTranslator", "&View");
constexpr auto kActionSelectLlm = QT_TRANSLATE_NOOP("UiTranslator", "Select &LLM…");
constexpr auto kActionClearCache = QT_TRANSLATE_NOOP("UiTranslator", "Clear cache…");
```

This matters because `lupdate` can now reliably see the active source strings for both top-level menus and the Settings-menu actions instead of depending on less direct wrapper usage.

The commit also adds:

- `aifilesorter_hi.ts`
- Hindi Quick Start Markdown
- build and resource wiring for the new translation/resource files
- wider tests for menu-title and menu-action translation coverage

## Validation

The work was validated with translation-target and UI-focused checks:

```bash
cmake --build build-tests --target aifilesorter_lrelease -j4
cmake --build build-tests --target ai_file_sorter_tests -j4
./build-tests/ai_file_sorter_tests "MainApp retranslate reflects language changes"
./build-tests/ai_file_sorter_tests "Top-level menu titles are translated for all supported UI languages"
./build-tests/ai_file_sorter_tests "Settings menu actions are translated for all supported UI languages"
./tests/run_translation_tests.sh
```

The translation sync script is also now explicitly aware of Hindi.

## User-visible impact

Users can select Hindi as an interface language, and the visible menu structure is much more consistently localized instead of partially falling back to English.

The Help quick-start content now also has a Hindi variant, so the language support is not limited to surface chrome.

## Remaining caveats

This commit substantially improves translation completeness, but translation quality remains a content-maintenance task. Future UI string additions still need matching catalog updates, which is why the stronger translation tests matter.

Hindi was added to the interface language list, not to category-language output choices. Category-language behavior continues to follow its own supported-language set.
