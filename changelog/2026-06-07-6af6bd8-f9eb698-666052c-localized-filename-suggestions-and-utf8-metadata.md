# Summary

This series made rename suggestions more language-aware and more Unicode-safe. Media metadata used for audio and video filename suggestions stopped dropping UTF-8 content, and the review pipeline gained filename localization so suggested names can follow the selected category language instead of always staying in English. The test catalog was updated at the same time.

# Motivation

The app had already learned how to translate categories and subcategories, but rename suggestions still lagged behind that model. Users choosing a non-English categorization language could still end up with English-oriented filename suggestions, and metadata-based rename parts were vulnerable to losing UTF-8 content when they were stitched into filenames.

# Implementation

`MediaRenameMetadataService` was tightened so UTF-8 metadata survives the composed rename path, and a dedicated `FilenameLocalizationService` was introduced to translate suggested filenames along the same category-language path as taxonomy labels. The cache still keeps canonical taxonomy labels in English where appropriate; the user-facing rename suggestion is the part that now localizes.

# Validation

Validation in this chapter came from targeted unit coverage:

- `tests/unit/test_media_rename_metadata_service.cpp`
- `tests/unit/test_whitelist_and_prompt.cpp`
- `TESTS.md` additions documenting UTF-8 filename analyzer coverage

# User-visible impact

Users working in French, Spanish, Korean, and other supported category languages can get rename suggestions that better match the language they selected, while media tags with non-ASCII content are less likely to be mangled or dropped in the proposed filename.

# Remaining caveats

Localized filename suggestions still depend on model output quality and the path-label sanitizer. The app is better at preserving and localizing UTF-8 content, but it still has to normalize unsafe filesystem characters before presenting a usable filename.
