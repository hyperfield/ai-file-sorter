# Storage plugin catalog runtime matching and sample catalog docs

Commits covered: `ec4653b`, `d82cfdb`, `89be02f`

## Summary

This commit group tightened the storage-plugin catalog workflow in three complementary ways:

- the app now reports a precise error when a remote catalog exists but contains no entry for the current runtime
- the script docs gained a checked-in sample storage catalog covering Linux, Windows, and macOS variants
- the repository ignore rules were extended so local script-example work does not pollute the worktree by default

Grouping these commits is justified because they all belong to the same operational story: authoring, testing, and troubleshooting runtime-specific storage plugin catalogs.

## Motivation

Once the app started selecting runtime-specific plugin manifests, a bad remote catalog could fail in a confusing way. A catalog with valid entries for other platforms is not a fetch failure, and it should not be reported like one.

At the same time, plugin authors needed a concrete sample catalog to copy, and the repository needed a way to avoid treating local example-work churn as ordinary source noise.

## Implementation

### Runtime mismatch reporting

`StoragePluginManifest.cpp` now distinguishes “catalog loaded, but nothing matches this runtime” from generic parse or download failure:

```cpp
if (filtered.empty() && had_entries && error && error->empty()) {
    *error = "Plugin catalog does not contain any entries for this runtime (" +
             storage_plugin_current_platform() + "/" +
             storage_plugin_current_architecture() + ").";
}
```

That gives the plugin manager a precise, user-actionable error string.

### Sample catalog

`app/scripts/examples/storage/catalog.sample.json` provides a concrete multi-runtime example for the OneDrive storage plugin, including Linux, Windows, macOS arm64, and macOS x86_64 entries. This is especially useful because the runtime matcher chooses the best manifest for the current platform/architecture pair.

### Ignore rule for local example churn

`.gitignore` was extended with `app/scripts/examples/`. In practice, that keeps local example and catalog experimentation from showing up in `git status` unless someone explicitly chooses to track it.

## Validation

Validation was both automated and documentation-oriented:

- `tests/unit/test_cache_interactions.cpp` gained a test that exercises the precise runtime-mismatch error
- `TESTS.md` was updated to document that coverage
- `app/scripts/README.md` now points directly at the sample catalog file

## User-visible impact

Users and plugin authors get better failure reporting:

- a remote catalog that only publishes the wrong runtime now says so explicitly
- plugin authors have a concrete sample format to copy

## Remaining caveats

- The ignore rule is convenient for local experimentation, but it also means new example files under that tree may need force-adding if they are intended to be committed.
- The sample catalog demonstrates shape and runtime matching, but it does not replace per-plugin release validation.
