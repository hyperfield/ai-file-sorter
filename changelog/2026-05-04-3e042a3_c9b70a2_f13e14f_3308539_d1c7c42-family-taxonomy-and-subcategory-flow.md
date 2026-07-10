# Family Taxonomy and Subcategory Flow

## Summary

Commits `3e042a3`, `c9b70a2`, `f13e14f`, `3308539`, and `d1c7c42` built out the repo’s family-based categorization phase before the later May cleanup pass:

- `3e042a3` introduced family-aware main-category selection and artifact normalization
- `c9b70a2` made subcategories enabled by default in settings
- `f13e14f` merged image and document category rules into a shared file-category policy
- `3308539` hardened split artifact subcategory parsing
- `d1c7c42` simplified and isolated split subcategory prompt handling

This sequence established the project’s “broad stable main category plus more specific leaf subcategory” behavior for images, documents, and software/archive-like artifacts.

## Motivation

Earlier categorization behavior had two recurring weaknesses:

- it was too easy for models to invent topical top-level categories such as `Security` or `Computing` instead of filesystem-friendly broad families
- split category/subcategory prompting for artifact files could drift or parse inconsistently, especially when models answered with awkward inline formats

The goal of this sequence was to make category selection more structural:

- choose stable main families from file type and extension clues
- steer specificity into the subcategory
- parse LLM category/subcategory output more defensively

## Implementation

### Shared family policy

The project now determines a file-family selection first and then constrains the main-category candidates around that family:

```cpp
switch (determine_file_family_kind(file_name)) {
    case FamilyKind::Image:
        return make_selection(FamilyKind::Image, image_categories());
    case FamilyKind::Document:
        return make_selection(FamilyKind::Document, document_categories());
    case FamilyKind::Software:
        return make_selection(FamilyKind::Software, software_categories());
```

This matters because image, document, audio, archive, software, and other artifact families are no longer just implied by prompt text. They feed a concrete main-category candidate list.

### Artifact subcategory normalization

Artifact handling now strips low-information or repeated family labels out of the subcategory:

```cpp
if (normalized_subcategory.empty() || is_low_information_artifact_label(normalized_subcategory)) {
    return kGeneralSubcategory;
}

if (normalized_subcategory == normalized_main ||
    (canonical_subcategory.has_value() &&
     normalize_match_text(*canonical_subcategory) == normalized_main)) {
    return kGeneralSubcategory;
}
```

That keeps answers like `Software / Software` or `Installers / Installer` from masquerading as useful leaf labels.

### Split response parsing hardening

The categorization service also became more resilient when parsing multi-line or inline LLM responses:

```cpp
for (const auto& entry : lines) {
    if (category.empty()) {
        if (auto value = extract_labeled_value(entry, {"category", "main category"}, true)) {
            category = std::move(*value);
        }
    }
    if (subcategory.empty()) {
        if (auto value = extract_labeled_value(entry, {"subcategory", "sub category"}, false)) {
            subcategory = std::move(*value);
        }
    }
}
```

The later `3308539` and `d1c7c42` work tightened this path further for artifact-specific split prompts and kept the parsing logic more isolated instead of smearing it across the broader categorization flow.

### Subcategories by default

`c9b70a2` completed the policy direction by making subcategories enabled by default in settings. That aligned the UI default with the new taxonomy model instead of forcing users to opt into the more specific leaf labels.

## Validation

This sequence added a large amount of targeted coverage, especially around:

- `tests/unit/test_whitelist_and_prompt.cpp`
- `tests/unit/test_cache_interactions.cpp`
- `tests/unit/test_local_llm_backend.cpp`
- `tests/unit/test_settings_image_options.cpp`

The changes were also validated against real categorization behavior because the whole point of the sequence was to improve filesystem-facing label stability, not just parser correctness in isolation.

## User-visible impact

Users should have seen a more opinionated categorization style:

- images stay under `Images`
- documents stay under `Documents`, `Presentations`, `Spreadsheets`, `Data Exports`, or `Configs`
- software and archive-like files bias toward stable families such as `Software`, `Installers`, `Drivers`, `Operating Systems`, and `Archives`
- specificity is pushed into the subcategory

Subcategories also became enabled by default, so the richer taxonomy shows up in fresh installs without extra settings changes.

## Remaining caveats

This sequence made the taxonomy flow more structured, but it also increased prompt and normalization complexity. That is part of why the later `64e73c8` cleanup simplified portions of the prompt behavior and hardened reset handling.

In other words, this sequence laid the structural groundwork, but it was not the end of the categorization-tuning story.
