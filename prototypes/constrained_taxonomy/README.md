# Constrained Taxonomy Prototype

This folder hosts experimental code for a "controlled vocabulary" categorization pipeline. The goal is to map detected file types to a curated set of `(category, subcategory)` pairs and keep LLM responses constrained to those options before we wire anything back into the Qt application.

## Layout

- `TaxonomyTemplatePrototype.cpp` – header-only style sandbox that demonstrates:
  - registering template lists for known file type fingerprints (extensions or MIME-ish hints);
  - building a prompt that only exposes those templates to the LLM;
  - interpreting the reply without touching the production `CategorizationService`.

## Usage Notes

This code does **not** build as part of `app/` yet. Compile/run it manually when iterating:

```bash
cd prototypes/constrained_taxonomy
c++ -std=c++20 TaxonomyTemplatePrototype.cpp -o taxonomy_prototype
./taxonomy_prototype /path/to/file.iso "Operating Systems"
```

The prototype depends only on the STL plus a couple of lightweight headers from `app/include/` so it can reuse existing type definitions without dragging in the full application build.

## Next Steps

- Flesh out the `kTemplates` map with real data pulled from production taxonomies.
- Integrate lightweight type detection (extensions + `file` command output) so the prototype can auto-select templates.
- Once stable, transplant the logic into `CategorizationService` behind a feature flag and re-enable the consistency pass / constrained prompts.
