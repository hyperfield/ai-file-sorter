# Retrieve learned taxonomy candidates

## Summary

Categorization now consults locally learned taxonomy candidates before asking the LLM to choose or generate categories.

## Motivation

Large whitelists and repeated user-approved categories should improve consistency without injecting every possible category into every prompt. Retrieval keeps prompts smaller and makes user-confirmed categories easier to reuse.

## Implementation

- Added a retrieval layer that builds candidates from learned taxonomy entries and file context.
- Passed only relevant learned or whitelist-backed candidates into categorization prompts.
- Preferred strong user-learned matches over generic or conflicting category guesses.
- Preserved the reviewability of results instead of silently forcing every learned match.

## Validation

Targeted categorization and learned-candidate tests were added and run. The full test suite was also run in the offscreen test environment after the feature track changes.

## User-visible impact

Users should see more consistent reuse of previously approved categories, especially when their whitelists are large or contain fine-grained category names.

## Remaining caveats

This does not train the LLM. It is retrieval-assisted categorization using local user data.
