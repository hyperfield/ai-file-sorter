# Summary

This group of commits expanded the app's multilingual surface in two directions: the interface gained Nordic and Simplified Chinese UI translations, and the category-language menu stopped treating every local model as if it supported the same output languages. The same batch also removed an unnecessary duplicate Gemma 4B visual download path and updated the README to match the new behavior.

# Motivation

Before these changes, the app could present category-language choices that were not a good fit for the selected local model, and it could store duplicate Gemma 4B visual artifacts even when the files were functionally the same. That created avoidable confusion in the UI and wasted disk space in local-model setups.

# Implementation

The category-language layer was generalized from a short fixed list to a much larger registry, then filtered through a new support helper that maps the active LLM choice to the languages that should actually be selectable in the menu. The UI translator was updated so localized language menus stay sorted consistently instead of drifting when translated labels change order.

On the runtime side, the LLM catalog stopped treating the shared Gemma 4B visual artifact as a separate download per entry. The dialog copy was also updated to make Gemma 3's multilingual categorization support explicit, so the menu logic and the user-facing explanation now point in the same direction.

```cpp
inline QString normalizeCategoryLanguageKey(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QChar('-'), QChar(' '));
    value.replace(QChar('_'), QChar(' '));
    return value.simplified();
}
```

This normalization step matters because the larger category-language set depends on stable matching for aliases and translated menu entries instead of a tiny hand-written mapping.

# Validation

Validation was mostly test-driven in this series:

- `tests/unit/test_category_language_support.cpp`
- `tests/unit/test_main_app_category_language_menu.cpp`
- `tests/unit/test_ui_translator.cpp`
- `tests/unit/test_custom_llm.cpp`

The README was also refreshed in the same batch so the documented language behavior matched the shipped UI.

# User-visible impact

Users now see a broader set of interface languages, more consistent language-menu ordering, and category-language choices that better match the selected local model. Gemma 4B visual users also avoid redundant downloads when the same shared artifact can be reused.

# Remaining caveats

The support filtering is capability metadata, not a live benchmark of model quality. A local model might technically generate a language outside its preferred set, but the menu now favors the combinations that the app explicitly supports and documents.
