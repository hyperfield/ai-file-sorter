# Storage Plugin Protocol

This document freezes the current external-process storage plugin contract for
`aifs-storage-plugin-v1`. It describes the JSON messages exchanged between AI
File Sorter and a storage connector process such as a cloud-storage helper.

Storage plugins are executable processes, not Qt/C++ in-process plugins. The app
starts the connector, writes one compact JSON request to stdin, closes stdin,
waits for the process to finish, and parses stdout as one JSON object. Stderr is
used only as diagnostic text when stdout is not valid JSON.

The machine-readable schemas live in:

- [`docs/schemas/storage-plugin-v1/request.schema.json`](schemas/storage-plugin-v1/request.schema.json)
- [`docs/schemas/storage-plugin-v1/response.schema.json`](schemas/storage-plugin-v1/response.schema.json)

## Package Trust

External-process storage connectors are distributed as signed `.aifsplugin`
archives. Before the host trusts a package manifest, it verifies
`plugin-signature.sig` over the exact bytes of `plugin-signature.json`, resolves
the signature `key_id` to a configured storage-package public key, and checks
the signed SHA-256 hash for every non-signature payload file. `manifest.json`
must be covered by the signed file list.

Package paths must be relative, use forward slashes, and stay inside the package
root. Storage manifests may use top-level `entry_point`/`package_paths` for a
single runtime or a `runtimes` array for platform- and architecture-specific
connector paths. Commercial metadata such as `license_required`, `product_id`,
and `purchase_url` lives in the signed manifest; user entitlement receipts are
checked separately from package trust.

## Compatibility Rules

- Requests always include `protocol`, `plugin_id`, `provider_id`, and `action`.
- `protocol` is currently `aifs-storage-plugin-v1`.
- `provider_id` may be empty for `probe`; provider-specific actions use the
  provider id from the plugin manifest.
- Responses should include `success`. A missing `success` is treated by the
  current host as success for legacy tolerance, but new connectors should always
  include it.
- Responses may include `protocol`. If present, it must be
  `aifs-storage-plugin-v1`; the current host also accepts responses with no
  protocol field.
- Unknown request or response properties are reserved for additive evolution and
  must be ignored by receivers that do not understand them.
- Failed responses set `success` to `false` and should include `error`; they may
  also include `error_code`.

## Conformance Harness

The protocol contract is exercised by the storage-plugin conformance harness,
not by sharing provider-specific C++ headers. The harness launches a candidate
connector executable, sends fixture requests for every v1 action, validates the
response shape expected for that action, and verifies basic fixture semantics
such as detection, listing, preflight checks, path existence, directory creation,
move, and undo.

Build the test target, then run it against a connector executable:

```bash
aifs_storage_plugin_conformance \
  --connector /path/to/connector \
  --provider-id mockcloud \
  --plugin-id mockcloud_storage_support
```

Use `--fixture-parent` to place the generated fixture tree under a provider
sync root, and `--allow-detect-miss` when testing response shape before a
connector has provider-specific detection available.

## Common Request Shape

```json
{
  "protocol": "aifs-storage-plugin-v1",
  "plugin_id": "mockcloud_storage_support",
  "provider_id": "mockcloud",
  "action": "inspect_path"
}
```

## Actions

| Action | Request payload | Success response payload |
| --- | --- | --- |
| `probe` | Base fields only. `provider_id` may be empty. | `provider_ids`, optional `plugin_id`, optional `supported_protocols`. |
| `detect` | `root_path` string. | `detection` object. |
| `capabilities` | Base fields only. | `capabilities` object. |
| `list_directory` | `directory` string and `options` bitmask. | `entries` array. |
| `inspect_path` | `path` string. | `status` object. |
| `preflight_move` | `source` and `destination` strings. | `preflight` object. |
| `path_exists` | `path` string. | `exists` boolean. |
| `ensure_directory` | `directory` string. | Base success response only. |
| `move_entry` | `source` and `destination` strings. | Mutation result fields. |
| `undo_move` | `source`, `destination`, and `created_directories`. | Mutation result fields. |

## File Scan Options

`list_directory.options` is the integer bitmask used by `FileScanOptions`.

| Bit | Value | Meaning |
| --- | ---: | --- |
| 0 | `1` | Include files. |
| 1 | `2` | Include directories. |
| 2 | `4` | Include hidden files. |
| 3 | `8` | Recurse into subdirectories. |

## Data Objects

### Detection

`detection.matched` decides whether the host treats the provider as applicable.
The host currently uses the provider id from the manifest rather than trusting a
provider id returned inside `detection`.

```json
{
  "provider_id": "mockcloud",
  "matched": true,
  "needs_additional_support": false,
  "confidence": 90,
  "detection_source": "sync_root",
  "message": "MockCloud folder detected."
}
```

### Capabilities

```json
{
  "supports_online_only_files": true,
  "supports_atomic_rename": false,
  "should_skip_reparse_points": true,
  "should_relax_undo_mtime_validation": true
}
```

### File Entry

```json
{
  "full_path": "C:/Users/example/MockCloud/file.txt",
  "file_name": "file.txt",
  "type": "File"
}
```

`type` is either `File` or `Directory`.

### Path Status

```json
{
  "exists": true,
  "hydration_required": false,
  "sync_locked": false,
  "conflict_copy": false,
  "should_retry": false,
  "retry_after_ms": 0,
  "stable_identity": "provider-specific-id",
  "revision_token": "provider-specific-revision",
  "message": ""
}
```

### Preflight

```json
{
  "allowed": true,
  "skipped": false,
  "hydration_required": false,
  "sync_locked": false,
  "destination_conflict": false,
  "should_retry": false,
  "retry_after_ms": 0,
  "source_status": {},
  "destination_status": {},
  "message": ""
}
```

`source_status` and `destination_status` use the path status shape.

### Mutation Result

`move_entry` and `undo_move` return mutation result fields at the response root.

```json
{
  "success": true,
  "mutation_success": true,
  "skipped": false,
  "message": "",
  "metadata": {
    "size_bytes": 1234,
    "mtime": 1780000000,
    "stable_identity": "provider-specific-id",
    "revision_token": "provider-specific-revision"
  }
}
```

When `mutation_success` is `false`, the host treats `skipped` and `message` as
the user-facing mutation result and reads missing metadata fields as default
zero or empty values.
