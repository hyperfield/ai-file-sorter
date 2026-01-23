# 2026-01-24 - Korean UI language, translation test coverage, mtmd CMake fix, language menu ordering

## Motivation

- **Korean UI support**: Users requested Korean as a selectable interface language. The app already supports multiple UI locales, so extending the existing translation mechanism is the most consistent path.
- **Test coverage for UI languages**: We added a new language, but the translation tests only validated English/French. Expanding the test to cover *all* UI languages reduces regressions in translation tables and ensures the menu selection logic remains correct.
- **CMake mtmd configure error**: The test build failed because the separately-added `mtmd` subdirectory didn’t inherit `LLAMA_INSTALL_VERSION`. That variable is needed for `set_target_properties(... VERSION ...)`. We fixed this at the app-level CMake without modifying the llama.cpp submodule.
- **Language menu ordering**: After adding Korean, the Interface Language menu wasn’t alphabetically sorted; this is a UX polish item that avoids confusion when scanning the list.

## What changed (high level)

1. Added **Korean** to the UI language enum and conversion helpers.
2. Added **Korean translations** (full translation table) and included Korean in the available language list.
3. Updated the **language menu** to insert Korean alphabetically.
4. Extended **UI translation tests** to validate *all* UI languages.
5. Fixed **mtmd build configuration** by passing the llama.cpp install version into the mtmd subdirectory.

## Detailed changes and rationale

### 1) Language enum + string conversion

**Why**: The UI language selector is driven by the `Language` enum and the `languageToString`/`languageFromString` helpers. We need Korean in all three places to make it selectable and persistent in settings.

**Excerpt** (`app/include/Language.hpp`):

```cpp
// Added Korean to the enum so it can be selected and persisted.
enum class Language {
    English,
    French,
    German,
    Italian,
    Spanish,
    Turkish,
    Korean,
    Dutch
};

inline QString languageToString(Language language)
{
    switch (language) {
    // ... existing cases ...
    case Language::Korean:
        return QStringLiteral("Korean");
    // ...
    }
}

inline Language languageFromString(const QString& value)
{
    const QString lowered = value.toLower();
    // ... existing cases ...
    if (lowered == QStringLiteral("korean") || lowered == QStringLiteral("ko")) {
        return Language::Korean;
    }
    return Language::English;
}
```

### 2) System locale default

**Why**: We map the OS locale to a default UI language in `Settings`. Adding Korean lets the app default to Korean on Korean systems.

**Excerpt** (`app/lib/Settings.cpp`):

```cpp
switch (QLocale::system().language()) {
    // ... existing cases ...
    case QLocale::Korean: return Language::Korean;
    // ...
}
```

### 3) Translation manager updates (Korean map + menu labels)

**Why**: The translation system is static and dictionary-based, so Korean needs its own translation map and must be listed as an available UI language. We also added `&Korean` to the other language maps so the “Interface language” submenu shows the translated label consistently across locales.

**Excerpt** (`app/lib/TranslationManager.cpp`):

```cpp
// New Korean translation table (sample)
static const QHash<QString, QString> kKoreanTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("폴더 분석")},
    {QStringLiteral("Folder:"), QStringLiteral("폴더:")},
    {QStringLiteral("Interface &language"), QStringLiteral("인터페이스 &언어")},
    {QStringLiteral("&Korean"), QStringLiteral("&한국어")},
    // ... full table continues ...
};

const QHash<QString, QString>* translations_for(Language lang)
{
    switch (lang) {
    // ... existing cases ...
    case Language::Korean: return &kKoreanTranslations;
    default: return nullptr;
    }
}

// Make Korean available in the language list
languages_.push_back(LanguageInfo{Language::Korean, QStringLiteral("ko"), QStringLiteral("Korean"), QString()});
```

### 4) Language menu ordering (alphabetical)

**Why**: The Interface Language menu is built in insertion order. We re-ordered the action creation so the list is alphabetical, improving scanability across all locales.

**Excerpt** (`app/lib/MainAppUiBuilder.cpp`):

```cpp
// Alphabetical insertion order: Dutch, English, French, German, Italian, Korean, Spanish, Turkish.
app.dutch_action = app.language_menu->addAction(QString());
// ...
app.english_action = app.language_menu->addAction(QString());
// ...
app.french_action = app.language_menu->addAction(QString());
// ...
app.german_action = app.language_menu->addAction(QString());
// ...
app.italian_action = app.language_menu->addAction(QString());
// ...
app.korean_action = app.language_menu->addAction(QString());
// ...
app.spanish_action = app.language_menu->addAction(QString());
// ...
app.turkish_action = app.language_menu->addAction(QString());
```

### 5) UI translation tests expanded to all languages

**Why**: We previously only validated English and French in the UI translation test. Adding Korean (and expanding the list) ensures every language has a known-good string for critical UI labels.

**Excerpt** (`tests/unit/test_main_app_translation.cpp`):

```cpp
struct ExpectedTranslation {
    Language language;
    QString analyze_label;
    QString folder_label;
};

const std::vector<ExpectedTranslation> expected = {
    {Language::English, QStringLiteral("Analyze folder"), QStringLiteral("Folder:")},
    {Language::French, QStringLiteral("Analyser le dossier"), QStringLiteral("Dossier :")},
    {Language::German, QStringLiteral("Ordner analysieren"), QStringLiteral("Ordner:")},
    {Language::Italian, QStringLiteral("Analizza cartella"), QStringLiteral("Cartella:")},
    {Language::Spanish, QStringLiteral("Analizar carpeta"), QStringLiteral("Carpeta:")},
    {Language::Dutch, QStringLiteral("Map analyseren"), QStringLiteral("Map:")},
    {Language::Turkish, QStringLiteral("Klasörü analiz et"), QStringLiteral("Klasör:")},
    {Language::Korean, QStringLiteral("폴더 분석"), QStringLiteral("폴더:")}
};
```

### 6) mtmd configure error fix

**Why**: `mtmd` is added as a separate CMake subdirectory in our project (outside the llama.cpp subproject), so its `CMakeLists.txt` didn’t receive `LLAMA_INSTALL_VERSION`. This caused a configuration error:

```
set_target_properties called with incorrect number of arguments.
```

We read the version from the llama.cpp build directory and pass it into the mtmd build before adding the subdirectory.

**Excerpt** (`app/CMakeLists.txt`):

```cmake
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp" "${CMAKE_CURRENT_BINARY_DIR}/llama-build")
get_directory_property(_llama_install_version DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/llama-build" DEFINITION LLAMA_INSTALL_VERSION)
if (NOT _llama_install_version OR _llama_install_version STREQUAL "LLAMA_INSTALL_VERSION-NOTFOUND")
    set(_llama_install_version "0.0.0")
endif()
set(LLAMA_INSTALL_VERSION "${_llama_install_version}")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/tools/mtmd" "${CMAKE_CURRENT_BINARY_DIR}/mtmd-build")
```

## Notes / Impact

- **UI Language**: Korean is now fully selectable and localized.
- **Tests**: One UI translation test covers all UI languages; future translations must provide at least the `Analyze folder` and `Folder:` strings to pass.
- **Builds**: The CMake configuration error for mtmd is resolved without modifying the llama.cpp submodule; this is safer for future submodule updates.
- **UI Polish**: Language menu is alphabetically ordered in all locales (insertion order is fixed, but labels are localized).
