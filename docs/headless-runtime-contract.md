# Headless Runtime Contract

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
