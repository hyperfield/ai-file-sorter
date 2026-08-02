# Architecture

AI File Sorter is a cross-platform Qt/C++ desktop application with one shared
analysis workflow and two main entry styles: the normal GUI flow and a
UI-neutral headless flow used by integrations.

## Main layers

- **UI layer**: `MainApp`, dialogs, menus, translations, preview widgets, and
  other user-facing Qt code.
- **Workflow/orchestration**: `AnalysisCoordinator` and
  `AnalysisWorkflowContext` coordinate scanning, categorization, review data,
  rename suggestions, and apply preparation without hard-coding a GUI-only host.
- **Categorization and naming**: `CategorizationService`,
  `LocalLLMPromptBuilder`, LLM client implementations, whitelist handling, and
  file-type policy code decide categories, subcategories, and supported rename
  suggestions.
- **Persistence and settings**: `Settings`, `DatabaseManager`, cache
  maintenance, learned behavior, and undo-plan storage persist local state.
- **Headless/integration contract**: `HeadlessAnalysisCommand`,
  `AnalysisRuntimeLock`, `HeadlessAnalysisWorkflowHost`, and
  `HeadlessReviewApplyService` expose a stable non-GUI contract for Explorer and
  future integrations.
- **Update and packaging surfaces**: updater/feed parsing, launcher behavior,
  build scripts, and platform-specific packaging support release flows.

## Architectural boundaries

- GUI code should remain optional. Headless callers should not need to
  instantiate dialogs or other UI classes.
- Workflow orchestration, settings/persistence, LLM prompting, and file-mutation
  logic should stay in separate modules with clear responsibilities.
- Explorer extension code belongs in the sibling repository
  `D:\projects\ai-file-sorter-explorer-extension`. This repository should expose
  only the app-side runtime and status behavior that the extension depends on.
- Windows-specific integration behavior must be guarded so macOS and Linux
  builds do not regress.
- New registry/settings/integration paths should use `HFStudio`. Compatibility
  code may still need to read or clean legacy `Quicknode` locations.

## Practical entry points

- **Normal app flow**: `MainApp` builds the UI, constructs services, and runs
  the shared analysis workflow through `AnalysisCoordinator`.
- **Headless analysis**: `HeadlessAnalysisCommand` parses CLI arguments, applies
  runtime locking, runs `HeadlessAnalysisWorkflowHost`, and emits machine-
  readable status JSON.
- **Headless apply**: `HeadlessReviewApplyService` replays saved review plans
  without rerunning the full analysis.
- **Prompt and taxonomy work**: `CategorizationService`,
  `LocalLLMPromptBuilder`, whitelist logic, and the related unit tests are the
  main surfaces to touch.

## Change guidelines

- Prefer small focused classes over adding unrelated responsibilities to large
  existing ones.
- Keep the headless contract stable when changing integration-adjacent code.
- Add or update focused tests when behavior changes, especially for prompt
  construction, settings persistence, cache behavior, estimators, and headless
  workflows.
