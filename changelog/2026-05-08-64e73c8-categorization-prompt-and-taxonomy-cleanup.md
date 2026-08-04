# Categorization Prompt and Taxonomy Cleanup

## Summary

Commit `64e73c8` simplifies categorization prompting and hardens cleanup of the persistent categorization state. The change is centered on three practical outcomes:

- the categorization flow moves back toward a simpler one-shot prompt style
- taxonomy resolution becomes less willing to distort repeated broad-family labels
- clearing the categorization cache can now wipe taxonomy and alias pollution as well

## Motivation

The app had started to accumulate two different kinds of categorization drift:

1. prompt complexity on generic files was making results less stable than older, simpler prompting
2. persistent taxonomy state could outlive cache clears, so polluted categories and aliases stayed in the database even after users tried to reset behavior

That combination was especially painful when a model produced a broad or slightly awkward label once and the database then kept reinforcing it.

## Implementation

### Safer fallback for repeated broad labels

The taxonomy resolver now treats a repeated broad family label as structural noise and degrades the subcategory to `General` instead of throwing the whole result away:

```cpp
if (const auto canonical_subcategory = canonicalize_broad_main_label(norm_subcategory);
    canonical_subcategory.has_value() &&
    canonical_subcategory->normalized == norm_category) {
    trimmed_subcategory = "General";
    norm_subcategory = normalize_label(trimmed_subcategory);
}
```

This matters for cases like `Audio / Audio` or other family-on-family pairs. The category remains meaningful while the subcategory becomes a safe fallback.

### Full taxonomy-aware cache reset

The cache clear path now has an explicit taxonomy-clearing mode:

```cpp
const char* delete_sql = clear_taxonomy
    ? "DELETE FROM category_translation;"
      "DELETE FROM category_alias;"
      "DELETE FROM category_taxonomy;"
      "DELETE FROM file_categorization;"
    : "DELETE FROM file_categorization;";
```

Before this change, clearing the categorization cache only removed `file_categorization` rows. Taxonomy, alias, and translation tables could stay polluted and continue affecting later runs.

### Prompt simplification and learned-state cleanup

The same commit also removes a chunk of split subcategory prompt machinery and adjusts the categorization pipeline to rely less on service-level prompt decomposition. The updated tests around cache interactions, prompt construction, and whitelist-imported learned state reflect that simplification.

## Validation

This change was validated mainly through targeted categorization and persistence coverage:

- `tests/unit/test_cache_interactions.cpp`
- `tests/unit/test_database_manager_rename_only.cpp`
- `tests/unit/test_user_learning_store.cpp`
- `tests/unit/test_whitelist_and_prompt.cpp`

During development, the resulting behavior was also checked against real cache/database output, because the main problem was not just unit-level correctness but whether polluted taxonomy state could still survive reset operations.

## User-visible impact

Users should get a cleaner recovery path when categorization drifts:

- clearing categorization cache now performs a real taxonomy reset
- broad repeated labels are more likely to fall back to `General` than to become unusable results
- simpler prompt behavior should reduce over-elaborate categorization drift on mixed non-image, non-document files

## Remaining caveats

This commit hardens the persistence and prompt layer, but it does not guarantee semantically perfect categorization. Model quality and artifact-specific naming still matter, and some categories may still require later normalization or policy refinement.

The taxonomy reset is stronger than before, so users who intentionally relied on organically learned taxonomy structure will lose more of that state when they choose a full cache clear.
