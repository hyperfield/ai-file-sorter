# Categorization Behavior

This page summarizes the app's categorization model, especially the parts that
are easy to lose inside a large README.

## Two categorization modes

### More refined

- Favors the most semantically accurate category and subcategory for the
  specific file.
- Is less constrained by stable top-level families such as `Documents` or
  `Images` when a more topic-specific category fits better.
- Typical outcomes: `Security / PCI DSS`, `Manuals / Camera Guides`,
  `Wildlife / Lions`.

### More consistent

- Favors stable filesystem-oriented top-level buckets across similar files.
- Reuses recent similar results as consistency hints when the evidence is close.
- Typical outcomes: `Documents / PCI DSS`, `Documents / Camera Guides`,
  `Images / Lions`.

## Supported document and image handling

- Supported document files receive document-specific prompt guidance.
- Supported image files receive image-specific prompt guidance.
- In `More consistent`, those prompt paths strongly prefer stable top-level
  families.
- In `More refined`, the prompt guidance is deliberately looser so semantically
  better top-level categories can survive when no whitelist forces them back.

## Whitelists

- Whitelists can constrain top-level categories only, global subcategories, or
  category-specific branching subcategories.
- Smart branching lets each main category have its own allowed subcategories.
- Whitelists apply in both categorization modes.
- Narrow whitelists are easiest for smaller models, but large whitelists are
  reduced before prompting so the model sees a more relevant candidate set.

## Category language behavior

- Categories are chosen canonically in English first.
- The UI then shows category labels in the selected category language.
- Suggested filenames for supported rename flows are localized the same way.

## Cache and learned behavior

- The categorization cache stores past results, rename suggestions, and
  consistency-oriented state so reruns are faster and steadier.
- Learned behavior is separate from the normal cache and comes from review
  decisions that the user explicitly approved.
- The cache acts as local memory for consistency. It does not train or modify
  the underlying model.

## Practical reading of the modes

- Use `More refined` when you want the folder structure to follow subject matter
  closely.
- Use `More consistent` when you want the broad folder layout to remain steady
  across mixed batches and repeated runs.
