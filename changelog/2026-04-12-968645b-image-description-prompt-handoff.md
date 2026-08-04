# Preserve image descriptions in the categorization handoff

Commits covered: `968645b`

## Summary

This commit fixed a real categorization bug in the image-analysis path: the visual model could produce a useful natural-language description, but that description was not being carried through cleanly into the later categorization prompt. The categorizer mostly saw the suggested filename, not the richer visual context.

## Motivation

This was the immediate cause of a lot of weak image categorization. Switching text models alone could not solve it because the prompt handoff itself was lossy. If the categorization LLM never receives the image description, it has far less information to work with and tends to fall back to generic buckets.

## Implementation

The fix added a dedicated image prompt-path builder that preserves both the suggested filename and the description payload:

```cpp
if (!normalized_description.empty()) {
    prompt_path += "\nImage description: " + normalized_description;
}
```

Three supporting changes matter here:

- `AnalysisCoordinator` now uses `build_image_prompt_path(...)` instead of only swapping in the suggested filename
- `CategorizationService` trims the progress-display version to the first line so the UI does not become noisy
- `LocalLLMClient::shrink_user_prompt_for_context(...)` now knows how to budget and truncate `Image description:` sections the same way it already handled `Document summary:`

That last point is important: once the description is preserved, it also has to survive prompt-compaction logic when contexts get tight.

## Validation

This fix was covered directly:

- `tests/unit/test_whitelist_and_prompt.cpp` now checks that image prompt paths contain the suggested filename and `Image description:`
- a second test verifies that prompt overrides passed into `CategorizationService` preserve that same payload
- `TESTS.md` was updated to document both cases

## User-visible impact

Image categorization gets better context:

- the categorizer sees the suggested filename
- it also sees the underlying visual description
- long descriptions are still trimmed deliberately when context budgets require it

In practice, this reduces the odds of generic image buckets caused by a thin handoff between vision analysis and categorization.

## Remaining caveats

- The quality of the result still depends on the visual model’s description quality.
- Very long descriptions can still be truncated by prompt-budget logic, but they are now truncated intentionally rather than being lost wholesale.
