# Summary

This fix separated generated date suffixes from canonical taxonomy storage. Image and document date suffixes are still available as folder-name overlays, but they no longer pollute the canonical cached category values that the consistency and recategorization logic relies on.

# Motivation

Date suffixes such as `_YYYY-MM-DD` for images and `_YYYY-MM` for documents are useful in destination folder names, but they are not true taxonomy labels. When those generated suffixes leak into cached canonical categories, later runs can treat display-only folder names as if they were the underlying category taxonomy.

# Implementation

The new `CategoryDateSuffix` helper centralizes append and strip logic for generated suffixes, and `AnalysisCoordinator` now strips generated suffixes back out when loading cached entries or preparing canonical category state. The display overlay remains reversible, but the stored taxonomy goes back to the unsuffixed base category.

```cpp
/**
 * Date suffixes are a reversible display/move-path overlay. They should not be
 * persisted as canonical category names.
 */
namespace CategoryDateSuffix {
```

That design comment in the new helper captures the point of the whole change: suffixes are presentation data, not canonical taxonomy.

# Validation

Validation came from:

- `tests/unit/test_category_date_suffix.cpp`
- `tests/unit/test_categorization_dialog.cpp`
- `TESTS.md` additions documenting the behavior

# User-visible impact

Users can keep date-enhanced folder names without teaching the cache that `Images_2026-06-28` is a distinct canonical category from `Images`.

# Remaining caveats

This fix only targets the generated suffix formats the app itself creates. If a user manually invents category names that happen to look like date suffixes, the canonicalization rules still have to make a best effort based on the known overlay formats.
