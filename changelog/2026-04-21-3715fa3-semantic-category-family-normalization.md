# Semantic Category Family Normalization

## Summary

Commit `3715fa3` added semantic family normalization for categories such as manuals, spreadsheets, backups, installers, drivers, firmware, guides, licenses, presentations, and ebooks. The goal is consistency without collapsing specialized categories into overly broad parent buckets like `Documents` or `Software`.

## Motivation

The app could previously assign labels such as `Manuals` to one set of files and `Documents` to another semantically equivalent set. Generic normalization helped with broad labels, but it also risked flattening useful category names. The desired behavior is to keep the same semantic group under one stable label while preserving specialized names where they matter.

## Implementation

`DatabaseManager::resolve_category` now checks a semantic-family table before final taxonomy matching. A family defines:

- the canonical category label to preserve
- the generic parent category it may appear under
- aliases for the specialized family
- generic parent aliases that can be treated as non-informative subcategories

For example, `Documents / Manuals`, `Manuals / General`, and `manuals / documents` can normalize to the same `Manuals / General` taxonomy entry. Software-like installer labels can similarly normalize to `Installers / General` instead of being flattened into `Software`.

The README was updated to describe taxonomy normalization and cached consistency hints as lightweight memory rather than LLM training.

## Validation

The commit updated `tests/unit/test_database_manager_rename_only.cpp` with cases that verify:

- backup labels preserve the `Backups` family
- generic document labels still normalize to `Documents`
- document-family labels such as `Manuals` and `Spreadsheets` are preserved
- installer labels are preserved under the `Installers` family
- unrelated software semantics remain under `Software`

## User-visible impact

Users should see more consistent folder suggestions for semantically equivalent files. Specialized folder names such as `Manuals`, `Spreadsheets`, or `Installers` are retained instead of being reduced to broad categories.

## Remaining caveats

The semantic families are currently defined in code. That keeps the first implementation simple, but future taxonomy updates may be easier if these definitions move to external data files.
