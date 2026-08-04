# llama.cpp Pointer and Screenshot Refresh

## Summary

Commit `728b7f4` updated the vendored `llama.cpp` pointer and refreshed screenshot assets related to the LLM selection UI.

## Motivation

The project regularly pins external runtime dependencies and screenshots so developer builds, documentation, and UI references stay aligned with the current app behavior. This commit was a small maintenance update in that sequence.

## Implementation

The commit adjusted `app/include/external/llama.cpp` and touched `images/screenshots/llm-select-win.png`. No application logic changed directly in this commit.

## Validation

No dedicated validation is visible from the commit contents beyond the dependency pointer and screenshot asset update. Any runtime validation would depend on subsequent build or smoke-test runs.

## User-visible impact

The only direct user-visible impact is documentation or screenshot freshness. Runtime behavior may be affected indirectly by the `llama.cpp` pointer, but this commit does not document a specific model-behavior change.

## Remaining caveats

Submodule pointer changes should be validated with the full local LLM build and at least one local inference path before release packaging.
