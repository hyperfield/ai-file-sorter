# Headless Runtime Contract

The integration-facing JSON contracts are available as machine-readable schemas:

- [Settings overrides](schemas/headless-settings-overrides.schema.json)
- [Review plans](schemas/headless-review-plan.schema.json)

The schemas describe recommended producer payloads. Individual settings are optional;
the parser tolerates invalid individual values as described below. Additional properties
allow compatible evolution. Applying a review always runs the app's validation again.

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

## Settings override contract

`--settings-overrides-file` reads a UTF-8 JSON object. Overrides affect only the current
process's settings overlay; they do not write `config.ini`. Except for the destination
root, omitted preferences inherit the app's persisted settings. Operation-specific
overlays still enforce categorization-only and rename-only behavior.

**Destination initialization is deliberately different from other preferences.** The host
loads app preferences, sets the analyzed folder, and clears the persisted destination
before applying explicit overrides. Omitting `destinationFolder`, or setting it to an
empty string, uses the analyzed folder (the common parent for explicit file selections).
The persisted **sorting mode remains inherited** unless `sortingMode` is supplied.
Consequently, an integration that wants independent settings must send all three of
`sortingMode`, `destinationFolder`, and `suggestNewFolders`, including an empty destination
when appropriate. No per-job config-file write is needed.

```json
{
  "sortingMode": "ExistingFolderTree",
  "destinationFolder": "D:\\Archive",
  "suggestNewFolders": false,
  "useSubcategories": true,
  "offerRenameImages": true,
  "imageFilenameWords": 3,
  "offerRenameDocuments": true,
  "documentFilenameWords": 5
}
```

| Properties | JSON type and meaning |
| --- | --- |
| `sortingMode` | String: `GeneratedCategories` or `ExistingFolderTree`. Producers should use these canonical names. |
| `destinationFolder` | String destination root; whitespace is trimmed. Empty explicitly selects the analyzed root. |
| `suggestNewFolders` | Boolean allowing missing folder-tree proposals, subject to review/apply validation. |
| `useSubcategories`, `useConsistencyHints` | Booleans controlling generated category layout and consistency. Subcategories do not alter literal folder-tree targets. |
| `useWhitelist`, `activeWhitelist` | Boolean activation and string whitelist name. |
| `allowedCategories`, `allowedSubcategories` | Arrays of direct whitelist strings; non-string and empty items are ignored and valid items are trimmed. |
| `categorizeFiles`, `categorizeDirectories`, `includeSubdirectories` | Boolean target-processing preferences. |
| `analyzeImagesByContent`, `processImagesOnly`, `addImageDateToCategory`, `addImageDatePlaceToFilename`, `offerRenameImages`, `renameImagesOnly` | Boolean image-workflow preferences. |
| `analyzeDocumentsByContent`, `processDocumentsOnly`, `offerRenameDocuments`, `renameDocumentsOnly`, `addDocumentDateToCategory` | Boolean document-workflow preferences. |
| `addAudioVideoMetadataToFilename` | Boolean audio/video naming preference. |
| `imageFilenameWords`, `documentFilenameWords` | Independent whole-number limits, described below. |
| `language`, `categoryLanguage` | String language display names. Category language affects category labels; image/document naming prompts request English descriptive words, preserving proper nouns and identifiers without translation. |

Boolean overrides require JSON booleans. Unrecognized sorting-mode strings, missing
values, and values of the wrong type are ignored; an invalid destination therefore still
uses the analyzed root. Unknown properties are ignored. Invalid JSON or a non-object root
is a file-level error, rather than a request to rewrite app preferences.

### Filename word-limit overrides

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

### Review plan JSON schema

Plans use `schemaVersion: 1`. New display metadata is optional within version 1 so
integrations can continue reading older plans. The full field definitions are in the
[review-plan schema](schemas/headless-review-plan.schema.json).

```json
{
  "schemaVersion": 1,
  "kind": "aifs.headlessReviewPlan",
  "operation": "categorize-and-rename",
  "jobId": "explorer-example",
  "paths": ["C:/Inbox/invoice.pdf"],
  "applyOptions": {
    "baseDir": "D:/Archive",
    "undoDir": "C:/Users/example/AppData/Local/AIFileSorter/undo",
    "useSubcategories": true,
    "includeSubdirectories": false,
    "applySuggestedNames": true,
    "moveCategorizedEntries": true,
    "allowNewFolderTargets": false,
    "categoryLanguage": "English"
  },
  "entries": [{
    "filePath": "C:/Inbox",
    "fileName": "invoice.pdf",
    "type": "file",
    "category": "Documents",
    "subcategory": "Invoices",
    "suggestedName": "Acme invoice 2026.pdf",
    "renameOnly": false,
    "renameApplied": false,
    "canonicalCategory": "Documents",
    "canonicalSubcategory": "Invoices",
    "folderTreeMode": true,
    "targetFolder": "Clients/Acme/Invoices",
    "targetFolderExists": true,
    "targetFolderSuggestedNew": false,
    "folderTreeAllowNewFolders": false,
    "plannedDestination": "D:/Archive/Clients/Acme/Invoices/Acme invoice 2026.pdf",
    "folderTreeRoutingExplanation": "The existing Acme invoice folder matches this document."
  }]
}
```

`filePath` is the **source parent directory**; the full source is `filePath / fileName`.
`applyOptions.baseDir` captures the resolved destination root. Generated-category entries
use category/subcategory labels under that root. Folder-tree entries use the literal
`targetFolder` path under that root, independently of those semantic labels. An empty
folder-tree target is unresolved, not a request to move to the root. Rename-only entries,
or plans with `moveCategorizedEntries: false`, remain in their source directory.

`applySuggestedNames` enables `suggestedName`, with the original extension preserved when
the suggestion lacks one. The app resolves duplicate names during the dry run and stores
the resulting names in the plan. `renameApplied` means the suggested rename has already
occurred. `useSubcategories` affects generated-category destinations only;
`includeSubdirectories` retains the analyzed scope. `undoDir` selects undo storage and
`categoryLanguage` supplies category-label context. The app reads the saved options for
apply rather than reloading current sorting settings or rerunning analysis.

`plannedDestination` is the full destination calculated by the app's dry run, including
name-collision handling. It may be absent in older/API-created plans or empty for a
skipped/invalid entry. Status entries expose the same full path as `destination`.
`folderTreeRoutingExplanation` is the app's explanation of its original route and also
appears on status entries. Treat both as display metadata. They never authorize a move,
select a target, or grant folder creation; the apply service ignores `plannedDestination`
and recomputes paths from the reviewed fields and options.

To approve a subset, retain only the selected entries in `entries`; do not use an
integration-specific checkbox property as an apply instruction. Preserve unknown fields.
After editing category labels, clear `canonicalCategory` and `canonicalSubcategory` so
stale canonical labels do not override the edits. After changing a destination, clear
`plannedDestination` and `folderTreeRoutingExplanation`; use an edited preview until the
app validates the new path. Existence flags describe the original snapshot, not a promise
that the filesystem still has that shape.

Missing folder-tree targets require `allowNewFolderTargets` or the entry's
`folderTreeAllowNewFolders`. The app revalidates relative paths, existence, naming rules,
and creation permission at apply time. Review and dry runs do not create folders or move
files. A routing explanation or `targetFolderSuggestedNew: true` cannot bypass this gate.

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
