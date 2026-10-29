# Headless Runtime Contract

## Reproducible Local Inference

Optional, per-process controls apply to native local text inference. They do not
change persisted preferences or shipped defaults. Invalid values fail before
model loading. Unset controls retain the existing behavior.

| Environment variable | Meaning |
| --- | --- |
| `AI_FILE_SORTER_LLM_TEMPERATURE` | Sampling temperature; `0` for greedy inference; default `0.8`. |
| `AI_FILE_SORTER_LLM_MIN_P` | Relative probability cutoff, from `0` to `1`; default `0.05`. |
| `AI_FILE_SORTER_LLM_SEED` | Unsigned 32-bit seed; unset uses a random seed. |
| `AI_FILE_SORTER_LLM_THREADS` | Generation and batch workers; `0` retains the runtime default. |
| `AI_FILE_SORTER_LLM_BATCH_TOKENS` | Logical prompt batch size; `0` retains the app default. |
| `AI_FILE_SORTER_LLM_MICROBATCH_TOKENS` | Physical prompt batch size; must not exceed an explicit logical batch. |
| `AI_FILE_SORTER_LLM_FLASH_ATTENTION` | `0` or `1`; unset retains the backend default. |
| `AI_FILE_SORTER_LLM_OUTPUT_TOKENS` | Explicit experiment budget replacing each text caller's requested output limit. |

These controls do not configure the image encoder/decoder. Context and GPU layer
controls remain separate. Input audits record the resolved sampling, workers and
actual token budgets; a fixed seed alone does not establish native/server parity.

## Existing-Folder Response Contract

Existing-tree routing first compares actual catalog paths (including pending
review proposals) and requires JSON with exactly `targetFolder` and `createFolder`.
Native text inference installs a per-request decoding grammar compiled by the
pinned llama.cpp JSON-schema converter. `targetFolder` is an enum of the literal
offered paths plus JSON `null`; `createFolder` must be `false`. Null explicitly
means no suitable offered destination, not the destination root. Comparisons that
exceed the measured input budget split whole candidate groups, never truncate
mandatory content evidence or paths.

The native converter follows the schema's `required` field sequence, retaining
`targetFolder` before `createFolder` as shown in the prompt examples, even when
the schema serializer sorts JSON object keys. This changes field presentation,
not the permitted paths, required fields or their types.

Only explicit abstention (or an empty catalog), with new-folder suggestions enabled, authorizes a
separate proposal request. That request requires a nonempty string path and
`createFolder:true`; path safety, naming conventions, number reservation and
equivalent pending-folder reuse still validate the proposal before review.
If a proposal names a literal existing catalog path, normal routing validates
and reuses that existing path with its actual existence flags. The optional
`verify_existing_proposals` control instead performs one bounded reconsideration
using the original evidence and folder purposes. Structural validity does not
establish semantic accuracy. No case, number or misspelled-parent repair occurs;
reconsideration cannot trigger another proposal loop.

Optional archive-local descriptions use the versioned format in
[Folder Purposes](folder-purposes.md). Descriptions are untrusted data and are
included in catalog fingerprints, so edits invalidate routing caches. Folder
purposes remain enabled in normal routing. The optional `compare_purpose` control
adds explicit artifact-kind, function/audience and subject comparison instructions.
No filename or extension maps to a hard-coded destination. Per-request
`FolderTreeRoutingOptions` permits isolated protocol measurements; production
defaults disable both `compare_purpose` and `verify_existing_proposals`.
Malformed replies do not authorize creation. Exhausted validation retries leave
the item unresolved, retaining the review/apply safety gate.

Remote/legacy clients without native constrained decoding retain strict output
validation and bounded retries; their token generation is not grammar-enforced.
Grammar setup failures never silently retry unconstrained or as a CPU fallback.
Ordinary classification, summaries and filenames remain unconstrained. Existing
routing cache records use a new policy marker so older combined-stage decisions
are not reused.

When input auditing is enabled, constrained native attempts include an optional
`responseConstraint` object with `enforced:true` and byte-exact `jsonSchema` and
`nativeGrammar` payloads (text, base64, length and SHA-256). It is recorded only
after installing the sampler, not merely because a constraint was requested.

This document summarizes the stable app-side contract for non-GUI integrations.
External callers should use the headless entry points rather than instantiating
GUI classes directly.

## Supported app-side entry points

- `HeadlessAnalysisCommand`
- `AnalysisRuntimeLock`
- `HeadlessAnalysisWorkflowHost`
- `HeadlessReviewApplyService`

## CLI contract

```text
Usage: aifilesorter --headless --operation <categorize|rename|categorize-and-rename> --path <file-or-folder> [--path <file-or-folder> ...] [--status-file <json-file>] [--job-id <id>] [--review-file <json-file>] [--review-only|--auto-apply] [--include-subdirectories|--no-include-subdirectories] [--settings-overrides-file <json-file>]
       aifilesorter --headless-apply --review-file <json-file> [--status-file <json-file>] [--job-id <id>]
```

## Filename word-limit overrides

`--settings-overrides-file` accepts two optional whole-number properties:

```json
{
  "imageFilenameWords": 6,
  "documentFilenameWords": 4
}
```

Each property independently controls the maximum content-derived words in suggested filenames.
The supported range is 3–6, with a persisted app default of 3. Integer values outside the range
are clamped. Omitted properties, nulls, strings, booleans, arrays, objects, fractional numbers,
and non-finite numbers leave the corresponding persisted preference in use. The host also
clamps overrides supplied through the in-memory API. These overlays apply only to the current
headless analysis; they never write the app config or modify a saved review plan's filenames.

## Supported targets

- One folder target.
- One or more explicit file targets that all live in the same parent folder.

## Unsupported targets

- Cross-folder file selections.
- Search-result style aggregations spanning multiple unrelated folders.

Those shapes are intentionally a later slice. Integrations should not assume
that arbitrary file collections are accepted.

## Operation semantics

- `categorize`: categorize and move according to the selected settings.
- `rename`: apply supported rename suggestions without moving files into
  category folders.
- `categorize-and-rename`: combine the two behaviors in one reviewed/applyable
  plan.
- `--headless-apply`: apply a previously saved review plan exactly, without
  rerunning analysis.

## Review and apply flow

- Integrations should expect a review-plan-capable workflow rather than silent
  mutation by default.
- `--review-only` forces review preparation without applying changes.
- `--auto-apply` opts into direct apply for callers that intentionally want it.
- Saved review plans use the JSON kind `aifs.headlessReviewPlan`.

## Status contract

- Machine-readable status is emitted to stdout and, when provided, to the status
  file path.
- Treat stdout/status-file JSON as the contract. Stderr is diagnostic output.
- Stable status values include `running`, `review_required`, `completed`, and
  `failed`.
- Review/apply payloads include review details plus `entryCount`,
  `movedCount`, `renamedCount`, and `skippedCount`.
- Running status updates may include partial review preview entries so a caller
  can show in-progress results before the full job is complete.

## Locking and concurrency

`AnalysisRuntimeLock` coordinates GUI, Explorer-worker, and headless runs.
Callers should not assume they can run concurrent analysis or mutation jobs
against the same runtime/state safely.

## Selection boundary

When a headless caller passes explicit file targets, the workflow may still scan
the parent folder internally, but review/apply output must remain filtered to
the selected file set.
