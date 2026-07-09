# Plugin build, publish, and catalog tooling

Commits covered: `219847b`, `4d42e65`, `7b698a1`, `4c4d25e`, `b57c0d9`

## Why this change was justified

A plugin system is not complete when the runtime can load plugins. It also needs a repeatable release pipeline. Without that, every plugin release becomes ad hoc manual work, and manual work quickly becomes broken work.

This group of commits built the operational tooling needed to:

- compile plugin binaries per platform
- package them into release payloads
- upload them to the remote server
- publish and consume a remote catalog
- keep names/layouts aligned with a more general plugin future rather than storage-only assumptions

The small `.gitignore` update is grouped here because it was part of that tooling hygiene.

## Build helpers for per-platform plugin binaries

The first step was to codify plugin builds across Linux, macOS, and Windows rather than relying on hand-written one-off commands.

```bash
# Example wrapper pattern introduced in the scripts layer
./app/scripts/build_plugins_linux.sh --plugins=onedrive_storage_support
```

That matters because plugin artifacts are platform-specific for the external-process model. A reproducible build interface is therefore not optional; it is part of the plugin delivery model.

The shared `plugin_build_targets.tsv` manifest further improved this by making plugin ids script-addressable instead of hard-coding target names into several scripts.

## Payload generation and root-level publishing

Next, the tooling learned to prepare a publishable payload under the repo-root `plugins/` tree.

```bash
./app/scripts/generate_plugin_payload.sh \
  --base-url=https://filesorter.app/download/plugins
```

The underlying idea is important:

- plugin packages, manifests, catalogs, and checksums should be generated together
- the generated URLs must match the final hosting layout
- release assets should be inspectable before upload

This is much safer than uploading a loose binary and hoping the remote catalog points to the same place.

## Upload tooling and actionable errors

The upload script did more than copy files. It validated the local payload, compared remote versions, and surfaced operational problems clearly.

```python
# Representative behavior from the uploader:
if remote_variant_missing:
    log_info(f"{plugin_key}: no remote entry for this runtime; will upload.")
```

Later improvements made the script more resilient and more explicit:

- plugin-root publishing instead of a hard-coded storage-only root
- better handling for missing remote directories
- clearer separation between public base URL and deploy destination
- more general script naming (`upload_plugins.py` rather than storage-only naming)

These are not trivial naming changes. They reflect an architectural decision that plugins may later span more than storage-provider functionality.

## Catalog URL wiring in the app

The tooling only works if the application knows where to find the remote catalog by default.

```cmake
set(AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL
    "https://filesorter.app/download/plugins/storage/catalog.json")
```

This was then mirrored into both:

- the CMake build path
- the legacy Makefile build path

That was justified because different build flows produce different binaries in this repository. If only one build path had the baked-in catalog URL, plugin behavior would differ depending on how the app was built and launched.

## Why the rename from storage-specific scripts mattered

The rename from `generate_storage_plugin_payload.sh` / `upload_storage_plugins.py` to more general names was not cosmetic. It corrected a conceptual mismatch:

- the current concrete use case is storage plugins
- the repository now has a plugin delivery pipeline that may later apply to other plugin categories too

Keeping “storage” in the top-level script names would have baked the current first use case into the long-term tool vocabulary.

## Representative code excerpts

### Generated payload layout as a deliberate contract

```text
plugins/
  storage/
    catalog.json
    SHA256SUMS
    onedrive/
      linux-x86_64/
        manifest.json
        onedrive_storage_support-1.1.0-linux-x86_64.aifsplugin
```

This layout is important because it solves several operational problems at once:

- per-runtime separation
- inspectable manifests
- one unified plugin root
- straightforward server hosting

### Upload script operational validation

```python
subprocess.run(["rsync", "-av", stage_root + "/", remote_dir + "/"], check=True)
```

The important part is not the `rsync` call itself; it is the policy around it:

- verify before upload
- compare against remote state
- fail with actionable diagnostics

That turns the uploader into release tooling rather than a glorified copy command.

## Net effect

By the end of this chapter:

- plugin builds were scriptable across operating systems
- payload generation produced a consistent publish tree
- the remote catalog could be baked into app builds
- upload tooling became operationally usable
- script naming/layout moved from “storage-only” to “plugin-capable”

This chapter completed the distribution side of the plugin story. Without it, the runtime/plugin manager would have remained a developer-only feature.
