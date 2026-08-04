# Changelog chapter instructions for AI File Sorter

These instructions apply to the detailed Markdown chapters stored in [`changelog/`](../changelog/). They are not a replacement for the short release bullets in [`README.md`](../README.md) or [`CHANGELOG.md`](../CHANGELOG.md); they are the long-form, commit-linked history for this repo.

## 1. File format and location

- Use Markdown (`.md`).
- Store each chapter directly under [`changelog/`](../changelog/).
- One file should usually describe one commit or one tightly related group of commits.

## 2. File naming

- Start the filename with the date in `YYYY-MM-DD` format.
- Include the short commit id or ids covered by the chapter.
- End with a short kebab-case description of the change.

Pattern:

```text
YYYY-MM-DD-<short-commit-id>[-<short-commit-id>...]-<short-description>.md
```

Examples:

- `2026-04-02-59d6737-plugin-system-foundation.md`
- `2026-04-02-dbc3295-92c8643-onedrive-plugin-sync-and-undo.md`

Guidance:

- Use `date +%F` to get the date.
- Use `git rev-parse --short HEAD` or the relevant commit ids from `git log`.
- Group multiple commits in one file only when they belong to the same logical change, such as implementation plus a small follow-up fix or tests.

## 3. What each chapter should explain

Each chapter should explain:

- what changed
- why the change was needed
- how it was implemented
- what the user-visible effect is
- what risks, tradeoffs, or compatibility concerns remain

Do not write generic “updated some code” notes. Tie the explanation to actual project behavior.

## 4. Project-specific details to cover

For AI File Sorter, include the relevant operational details when they apply:

- local LLM behavior
  - selected/default models
  - `llama.cpp` integration changes
  - downloader / model catalog behavior
  - CUDA / Vulkan / Metal / CPU backend handling
- remote model behavior
  - OpenAI / Gemini / OpenAI-compatible server changes
  - API key storage or validation changes
- categorization behavior
  - whitelists
  - multilingual categorization changes
  - consistency mode vs refined mode
  - cache effects and migration behavior
- rename behavior
  - image rename suggestions
  - document rename suggestions
  - audio/video metadata rename behavior
- platform-specific behavior
  - Windows / macOS / Linux differences
  - packaging and installer changes
  - startup/bootstrap/runtime library changes
- plugins
  - install/update/remove behavior
  - compatibility and storage-provider specifics
- persistence / settings
  - config migrations
  - cache schema changes
  - undo behavior

If a change affects only one platform or one runtime path, state that explicitly.

## 5. Code excerpts

- Include short, relevant code excerpts when they help explain the implementation.
- Prefer excerpts that show the key decision or algorithm, not large dumps.
- Add a short explanation below each excerpt explaining why that code matters.
- Keep excerpts accurate and current.

Do not include long pasted files when a focused snippet is enough.

## 6. Testing and validation

Each chapter should mention how the change was validated when that information is available, for example:

- manual UI verification
- specific unit or integration tests
- platform-specific smoke checks
- build-system validation
- model download / inference validation

If validation was limited, say so clearly.

## 7. Relationship to release notes

- Use [`README.md`](../README.md) and [`CHANGELOG.md`](../CHANGELOG.md) for short release-facing summaries.
- Use these chapter files for detailed implementation history.
- If a chapter describes user-visible behavior that should appear in release notes, make sure the shorter docs are updated too.

## 8. Writing style

- Be detailed, but stay concrete.
- Prefer repository-specific explanation over textbook exposition.
- Explain cause and effect.
- Avoid filler and repeated background.
- Use headings when they improve scanability.

Good structure for a chapter:

1. Summary
2. Motivation
3. Implementation
4. Validation
5. User-visible impact
6. Remaining caveats

## 9. Things to avoid

- vague “refactor” descriptions with no outcome
- huge unannotated code dumps
- describing changes without explaining motivation
- claiming broad platform support when the change was tested only narrowly
- omitting migration or compatibility implications when settings, caches, plugins, or model/runtime paths changed
