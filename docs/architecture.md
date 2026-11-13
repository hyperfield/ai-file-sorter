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
  suggestions. `DocumentTextAnalyzer` extracts useful text and normalizes document
  names; `DocumentSummaryService` handles budget fitting, chunking, and summary
  merging independently of extraction and filename cleanup.
- **Visual backends**: `VisualLlmRuntime` resolves either local model/projector
  artifacts or an explicitly enabled custom API connection. `ImageAnalyzerFactory`
  selects `LlavaImageAnalyzer` or `HttpImageAnalyzer`; both use
  `ImageFilenamePolicy`. `ChatCompletionTransport` owns HTTP authentication,
  timeouts, cancellation, HTTP errors and assistant-text parsing for compatible
  text and vision clients. The workflow caches API image evidence by connection,
  URL, model and filename word limit, without including credentials.
- **Persistence and settings**: `Settings`, `DatabaseManager`, cache
  maintenance, learned behavior, and undo-plan storage persist local state.
- **Headless/integration contract**: `HeadlessAnalysisCommand`,
  `AnalysisRuntimeLock`, `HeadlessAnalysisWorkflowHost`, and
  `HeadlessReviewApplyService` expose a stable non-GUI contract for Explorer and
  future integrations.
- **Update and packaging surfaces**: updater/feed parsing, launcher behavior,
  build scripts, and platform-specific packaging support release flows.
- **TLS trust**: `CurlTls` configures certificate and hostname
  verification for HTTP clients and HTTPS proxies. Linux retains libcurl's
  distribution-configured CA locations; runtime packages require
  `ca-certificates`. macOS requires active Secure Transport, clears
  file/blob trust overrides, and validates through Apple's trust service.
  Startup selects Secure Transport before curl initialization when available;
  capability checks inspect the active backend, excluding parenthesized inactive
  entries reported by MultiSSL builds. Startup logs the platform trust policy
  and linked curl backend; macOS initialization failures reach the core log.
  Windows requires active Schannel, uses native Windows roots and distrust policy,
  clears origin/proxy file/blob anchors, and enforces strict revocation at both
  TLS layers. All eight HTTP implementations apply the shared policy, including
  HTTP-to-HTTPS redirects, HEAD requests and retries. Windows startup checks
  initialization and logs the loaded curl DLL path; Windows build checks run the
  production TLS self-test and reject competing curl DLLs. Reused staging layouts
  remove the obsolete adjacent CA file. Old user-data CA caches are inert.
  No platform embeds, stages or installs an application CA snapshot.
  CMake, Make, and macOS packaging check the selected runtime. AppleSecTrust alone does
  not establish exclusive native trust: OpenSSL can accept additional anchors
  from its default paths or environment. Those alternate runtimes fail closed.

- Photo date extraction stays local. The existing date/place filename preference
  gates both coordinator enrichment and the metadata service's online-place
  permission, which defaults off. A date-only category preference cannot enable
  geocoding. Requests and cache keys share coordinates rounded to three decimal
  places using the classic numeric locale; no raw EXIF coordinates enter the
  request URL. Rounding reduces precision but does not guarantee anonymity.

## Architectural boundaries

- GUI code should remain optional. Headless callers should not need to
  instantiate dialogs or other UI classes.
- Filesystem-facing APIs use native paths; string interfaces carry UTF-8.
  Windows environment and command-line arguments are decoded from their wide
  representations before parsing, including before Qt startup. Log file streams
  use native filenames without changing the linked spdlog filename ABI.
- ASCII protocol operations and numeric parsing are locale-independent.
  `TextEncoding` owns Unicode letter, grapheme-length and place-prefix policies.
  Category labels allow 80 grapheme clusters subject to native component limits.
- `TranslationManager` accepts `QCoreApplication`, allowing headless display
  messages to use saved/overridden language preferences. GUI startup installs
  translations before its first dialog. Qt standard catalogs are embedded;
  `AnalysisProgressTranslation` resolves coordinator and shared-label contexts.
- Workflow orchestration, settings/persistence, LLM prompting, and file-mutation
  logic should stay in separate modules with clear responsibilities.
- Explorer extension code belongs in the sibling repository
  `D:\projects\ai-file-sorter-explorer-extension`. This repository should expose
  only the app-side runtime and status behavior that the extension depends on.
- Windows-specific integration behavior must be guarded so macOS and Linux
  builds do not regress.
- MSVC/precompiled Windows builds delay the main executable's GGML/llama imports
  until Vulkan validation and CPU fallback complete, then preload the libraries
  explicitly from the selected runtime directory. This also protects the direct
  Store executable from Vulkan import failures before `main`.
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
