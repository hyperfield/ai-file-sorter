# Recategorization, Whitelists, Dialogs, and Platform Polish

## Summary

Commits `83f29d6`, `75acfc4`, `871d3c3`, `71c85be`, `a743600`, `b053c3e`, `a2603d6`, `867e0bc`, `716e036`, `5394320`, and `0613407` make up a broad late-2025 stabilization pass:

- offer recategorization when the stored categorization mode differs
- add row sorting in the categorization review dialog
- refactor translation-manager internals
- expand whitelist-focused tests and prompt coverage
- fix Linux remote-LLM selection dialog OK-button activation
- improve Windows Vulkan launcher handling
- auto-expand file-view columns
- remove the splash screen
- make a few Linux-side app tweaks
- continue test growth around downloader and UI translator behavior

## Motivation

By mid-November 2025, the project had several adjacent quality concerns:

- categorization results needed better review-time controls
- whitelist and prompt behavior needed stronger automated coverage
- the Linux remote-LLM dialog had an annoying confirmation-state bug
- Windows Vulkan launcher behavior still needed improvement
- startup and table-view polish were overdue

## Implementation

This cluster spans several modules rather than one algorithm:

- `DatabaseManager`, `MainApp`, and `CategorizationService` for recategorization-mode awareness
- `CategorizationDialog` for row sorting
- `TranslationManager` refactoring
- whitelist/prompt test additions and related service adjustments
- `LLMSelectionDialog` fixes for Linux remote selection
- `startapp_windows.cpp` Vulkan launcher improvements
- `MainApp.cpp` file-view column expansion
- `main.cpp` removal of the splash screen
- further downloader and UI translator tests in early December

## Validation

Validation here was a mix of:

- UI verification of recategorization prompts, sorting, and dialog state
- whitelist and prompt tests
- Linux dialog behavior checks
- Windows Vulkan launcher smoke checks
- broader automated coverage growth for downloader and translation UI

## User-visible impact

Users saw several quality-of-life improvements:

- clearer recategorization behavior when modes differ
- sortable review rows
- a fixed OK button in the Linux remote-LLM dialog
- a simpler startup flow without the splash screen
- clearer file-view columns

## Remaining caveats

This chapter groups a wide stabilization band rather than one narrow subsystem. The benefit is that the late-2025 quality work is recorded; the tradeoff is that some smaller implementation details are intentionally summarized rather than described commit by commit.
