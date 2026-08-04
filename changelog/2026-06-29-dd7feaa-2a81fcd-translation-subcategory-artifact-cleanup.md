# Summary

These two fixes cleaned up a subtle multilingual parsing bug: category and subcategory text coming back from translation steps could still contain inline label artifacts such as `subcategory:` or `main category:`. The app now strips those fragments both while parsing live model output and while persisting translated taxonomy labels.

# Motivation

As category-language support grew, some translation or model-output paths started returning strings that mixed the value with an inline label, for example a translated category followed by `, subcategory: ...`. If those strings reached the taxonomy database unchanged, users could end up with noisy or malformed category labels in non-English flows.

# Implementation

The cleanup logic was strengthened in three places:

- `CategorizationService`
- `LocalLLMClient`
- `DatabaseManager` translation upserts

The parser now trims artifact labels not only when they appear at the start of a value, but also when they appear after delimiters such as commas, semicolons, or dashes.

```cpp
if (const auto delimited_cut = find_delimited_label(value, delimited_labels);
    delimited_cut != std::string::npos) {
    value.resize(delimited_cut);
}
```

That extra delimiter-aware cut is what catches values that were mostly correct except for a trailing embedded label fragment.

# Validation

Validation came from new and expanded coverage in:

- `tests/unit/test_whitelist_and_prompt.cpp`
- `tests/unit/test_database_manager_rename_only.cpp`
- `TESTS.md`

# User-visible impact

Translated category and subcategory labels are less likely to include stray `category` or `subcategory` fragments, which makes multilingual review flows and stored translations cleaner.

# Remaining caveats

This cleanup targets the common label-artifact patterns the app has seen in practice. Extremely unusual model output can still require future parser tweaks, especially in multilingual prompt chains.
