# Summary

This pair of commits upgraded whitelist handling from flat category/subcategory lists to smart branching constraints. Users can now define subcategories per category, and both the prompt builder and the whitelist editor understand that structure instead of forcing a single global subcategory list.

# Motivation

Flat whitelist lists work for small taxonomies, but they become awkward when the same subcategory should be legal only under one main category and not another. Without category-specific branching, the prompt either over-permits subcategories or forces users to keep duplicating and hand-editing lists.

# Implementation

The data model gained `subcategories_by_category`, the prompt-building path started deriving effective subcategory choices from the chosen main category, and the whitelist editor was rebuilt so users can switch between a global subcategory list and category-specific rows. The editor intentionally makes those two modes exclusive so the meaning of a whitelist stays unambiguous.

```cpp
std::vector<std::string> effective_subcategories_for_category(
    const std::string& category,
    const std::vector<std::string>& flat_subcategories,
    const std::unordered_map<std::string, std::vector<std::string>>& subcategories_by_category)
{
    if (auto mapped = explicit_subcategories_for_category(category, subcategories_by_category)) {
        return *mapped;
    }
    return flat_subcategories;
}
```

This helper is the core behavior change: once a category has its own subcategory branch, the prompt and validation logic stop falling back to an unrelated global list.

# Validation

Validation came from significant new coverage in:

- `tests/unit/test_whitelist_and_prompt.cpp`
- `AppTestRunner` whitelist fixtures
- `TESTS.md`

The UI side was validated through the reworked `WhitelistManagerDialog` behavior and its translation updates.

# User-visible impact

Whitelist users can express richer taxonomy rules, and the app can enforce those rules in both the editor and the categorization prompt. That makes large, structured taxonomies more practical without over-constraining unrelated categories.

# Remaining caveats

The feature increases whitelist complexity. It is more expressive, but users maintaining very large branching taxonomies still need to keep the category-specific rows consistent over time.
