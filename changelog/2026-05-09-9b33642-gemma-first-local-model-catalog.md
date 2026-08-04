# Gemma-First Local Model Catalog

## Summary

Commit `9b33642` changes the built-in local categorization model catalog to a Gemma-first lineup:

1. Gemma 3 4B IT Q4_K_M
2. Mistral 7B Instruct v0.2 Q5_K_M
3. Gemma 1.1 7B IT Q5_K_M

It also preserves the legacy built-in LLaMa entry for users who already downloaded the old artifact, while moving new defaults and settings migration to the Gemma 4B path.

## Motivation

The local-model selection dialog and the built-in defaults had drifted out of sync with the models the project now wants to present first. The app needed to do three things at once:

- change the default local categorization choice to Gemma 4B
- keep Mistral 7B available as the established 7B option
- avoid breaking users who already had the old built-in LLaMa 3B artifact on disk

Without a proper catalog abstraction, those concerns were scattered across the dialog, settings, startup readiness checks, and visual-runtime availability checks.

## Implementation

The built-in local model metadata now lives in one catalog table:

```cpp
static const std::vector<DefaultLlmEntry> entries = {
    {LLMChoice::Local_4b_Gemma, "LOCAL_LLM_3B_DOWNLOAD_URL", "LOCAL_LLM_3B_DISPLAY_NAME",
     "Gemma 3 4B IT Q4_K_M"},
    {LLMChoice::Local_7b, "LOCAL_LLM_7B_DOWNLOAD_URL", "LOCAL_LLM_7B_DISPLAY_NAME",
     "Mistral 7B Instruct v0.2 Q5_K_M"},
    {LLMChoice::Local_7b_Gemma, "LOCAL_LLM_7B_GEMMA_DOWNLOAD_URL",
     "LOCAL_LLM_7B_GEMMA_DISPLAY_NAME", "Gemma 1.1 7B IT Q5_K_M"},
    {LLMChoice::Local_3b_legacy, "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL",
     "LOCAL_LLM_3B_LEGACY_DISPLAY_NAME", "LLaMa 3b v3.2 Instruct Q8, legacy"}};
```

That matters because the dialog, startup checks, settings migration, and runtime artifact detection can now share one source of truth instead of hard-coding model identities in multiple places.

Legacy compatibility is handled through explicit fallback path resolution:

```cpp
if (choice == LLMChoice::Local_3b_legacy) {
    append_unique_path(paths, path_from_url(kLegacyLlama3BQ4Url));
}
```

The saved setting `Local_3b` now migrates to `Local_4b_Gemma`, while users who explicitly still have the legacy artifact can keep using `Local_3b_legacy`.

## Validation

The commit added and exercised targeted coverage for:

- settings migration from `Local_3b` to `Local_4b_Gemma`
- persistence of `Local_7b_Gemma`
- correct built-in dialog ordering
- legacy-artifact detection without falsely marking Gemma 4B as downloaded

The development checks for this work included:

```bash
cmake --build build-tests --target ai_file_sorter_tests -j4
./build-tests/ai_file_sorter_tests "Settings maps legacy Local_3b choices to Gemma 4B"
./build-tests/ai_file_sorter_tests "Built-in Gemma 7B choice persists across Settings load/save"
./build-tests/ai_file_sorter_tests "LLM selection dialog*"
```

## User-visible impact

Users now see a clearer built-in local model order in the selection dialog, with Gemma 4B first. New defaults and downloads align with that presentation.

Users who already have the old built-in LLaMa file are not forced into a silent broken state. The legacy entry can still resolve the old artifact, but it no longer blocks the newer Gemma-first catalog layout.

## Remaining caveats

The Gemma 4B base GGUF is shared with the visual Gemma setup, but categorization still uses it as a text model without `mmproj`. That is intentional, but it can be confusing if users assume any Gemma 4B artifact on disk must be visual-only.

This commit updates the built-in catalog, not the quality characteristics of the models themselves. Differences in semantic categorization quality still depend on the chosen model and current prompt/policy behavior.
