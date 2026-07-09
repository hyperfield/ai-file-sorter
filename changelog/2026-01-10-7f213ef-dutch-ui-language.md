# 2026-01-10 - Dutch UI language support

This changelog documents the addition of Dutch as a selectable interface language.
The goal is to let the UI labels, menus, dialogs, and status strings render in
Dutch while keeping English as the fallback when no translation exists.

## Why this change

- Users asked for a Dutch interface option, in addition to existing UI languages.
- The UI already uses a static translation map, so adding Dutch cleanly fits into
  the existing localization architecture without introducing new runtime tooling.

## What was added

### 1) New language enum + settings mapping

Dutch is now part of the UI language enum and can be loaded/saved from `config.ini`.
This ensures that once a user chooses Dutch, the preference persists between runs.

```cpp
// Language.hpp
enum class Language {
    English,
    French,
    German,
    Italian,
    Spanish,
    Turkish,
    Dutch // new: UI language selection uses this enum value
};
```

```cpp
// Settings.cpp
switch (QLocale::system().language()) {
    case QLocale::Dutch: return Language::Dutch; // new: system locale -> Dutch
    default: return Language::English;
}
```

### 2) Dutch option in the Interface language menu

The UI language menu now includes “Dutch”, wired into the same QActionGroup as
other UI languages, so the change flows through the existing signal path.

```cpp
// MainAppUiBuilder.cpp
app.dutch_action = app.language_menu->addAction(QString());
app.dutch_action->setCheckable(true);
app.dutch_action->setData(static_cast<int>(Language::Dutch)); // used by QActionGroup
app.language_group->addAction(app.dutch_action);
```

### 3) Dutch strings in the translation map

A full `kDutchTranslations` map was added, mirroring the other UI languages and
covering the core UI strings plus recent additions (image analysis options,
review dialog labels, and download prompts).

```cpp
// TranslationManager.cpp
static const QHash<QString, QString> kDutchTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("Map analyseren")},
    {QStringLiteral("Use subcategories"), QStringLiteral("Subcategorieën gebruiken")},
    {QStringLiteral("Review and Confirm"), QStringLiteral("Beoordelen en bevestigen")},
    {QStringLiteral("Analyze picture files by content (can be slow)"),
     QStringLiteral("Afbeeldingsbestanden op inhoud analyseren (kan traag zijn)")}
    // ...additional UI strings translated to Dutch
};

const QHash<QString, QString>* translations_for(Language lang)
{
    switch (lang) {
        case Language::Dutch: return &kDutchTranslations; // new: route Dutch lookups
        default: return nullptr;
    }
}
```

### 4) Dutch in the existing .ts files

Each existing locale now includes the “&Dutch” menu label so it can be shown
correctly in French, German, Italian, Spanish, and Turkish. This keeps the
language selector itself localized when users are already in another UI language.

Example (French):

```xml
<message><source>&amp;Dutch</source><translation>&amp;Néerlandais</translation></message>
```

### 5) New Dutch .ts file

A dedicated Qt Linguist source file was added: `app/resources/i18n/aifilesorter_nl.ts`.
This mirrors the translation map and ensures Dutch is available for workflows
that prefer `.ts/.qm` artifacts.

> Note: this change adds the `.ts` file only. If you rely on `.qm` resources,
> regenerate them with `lrelease app/resources/i18n/aifilesorter_*.ts`.

## Summary of user-visible behavior

- A “Dutch” option appears under **Settings → Interface language**.
- Selecting Dutch immediately re-translates the UI (menus, labels, status text).
- The setting persists in `config.ini` and is restored on the next launch.

## Files involved

- `app/include/Language.hpp` (enum + string mappings)
- `app/lib/Settings.cpp` (system default language detection)
- `app/lib/MainAppUiBuilder.cpp` (menu action wiring)
- `app/lib/TranslationManager.cpp` (Dutch translation map)
- `app/resources/i18n/aifilesorter_nl.ts` (Dutch Qt translation source)
- `app/resources/i18n/aifilesorter_{fr,de,it,es,tr}.ts` (localized “Dutch” label)
