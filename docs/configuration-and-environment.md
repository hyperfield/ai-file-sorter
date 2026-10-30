# Configuration and Environment

This page collects the configuration surfaces that are most useful for
contributors, packagers, and integrators. The main README can stay focused on
normal setup and usage.

## Local settings and storage

- `config.ini` lives under the app config directory.
- `AI_FILE_SORTER_CONFIG_DIR` overrides the base config directory.
- `AI_FILE_SORTER_LLM_STORAGE_DIR` overrides where downloaded local model files
  are stored.
- `AI_FILE_SORTER_LLM_DIR` is the legacy alias for
  `AI_FILE_SORTER_LLM_STORAGE_DIR`.
- `CATEGORIZATION_CACHE_FILE` overrides the categorization SQLite filename
  inside the config directory.

## Runtime backend selection

- The Windows and Linux launchers support `--cuda={on|off}` and
  `--vulkan={on|off}`.
- `AI_FILE_SORTER_GPU_BACKEND` selects `auto`, `cuda`, `vulkan`, or `cpu`.
- `AI_FILE_SORTER_N_GPU_LAYERS` overrides llama.cpp GPU offload selection.
- `AI_FILE_SORTER_CTX_TOKENS` overrides local LLM context length.
- `AI_FILE_SORTER_GGML_DIR` points to a custom ggml runtime directory when the
  packaged/default discovery path is not the right one.

## Visual-model configuration

- `LLAVA_MODEL_URL` and `LLAVA_MMPROJ_URL` override the built-in LLaVA download
  URLs.
- `GEMMA3_4B_MODEL_URL` and `GEMMA3_4B_MMPROJ_URL` override the built-in Gemma
  visual-model download URLs.
- `AI_FILE_SORTER_VISUAL_USE_GPU=0` forces the visual encoder to stay on CPU.

## Timeouts, pacing, and logs

- `AI_FILE_SORTER_LOCAL_LLM_TIMEOUT`
- `AI_FILE_SORTER_REMOTE_LLM_TIMEOUT`
- `AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT`
- `AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE`
- `AI_FILE_SORTER_LLAMA_LOGS`

These knobs are most useful when diagnosing slow providers, rate-limited
providers, or local runtime issues.

## Headless setting overlays

Headless callers can pass `--settings-overrides-file <json-file>` to inject a
non-persistent settings overlay for one run. This is the preferred way to steer
integration-specific behavior without rewriting the user's saved settings.

## Linux distro repository build profile

Packagers targeting official Linux repositories can configure a repository-safe
plugin profile with:

```sh
cmake -S app -B build-distro \
  -DAI_FILE_SORTER_DISTRO_REPOSITORY_BUILD=ON \
  -DAI_FILE_SORTER_UPDATE_MODE=DISABLED
```

`AI_FILE_SORTER_DISTRO_REPOSITORY_BUILD=ON` preserves local signed plugin
management, including install-from-file and per-plugin enable/disable controls,
but disables remote plugin catalogs, catalog-delivered plugin downloads/updates,
in-app commercial plugin obtain links, Johnny.Decimal purchase/install prompts,
and plugin license activation. Normal source/download builds leave this option
off and keep the standard plugin catalog and activation behavior.

The distro profile also changes dependency defaults that are normally convenient
for upstream binary builds:

- `AI_FILE_SORTER_PDF_BACKEND=pdftotext` by default. Supported values are
  `embedded-pdfium`, `system-pdfium`, `pdftotext`, and `auto`.
- `AI_FILE_SORTER_LLAMA_PROVIDER=system` by default on Linux distro builds.
  Supported values are `precompiled`, `bundled-source`, and `system`.
- `AI_FILE_SORTER_LLAMA_BACKENDS=cpu` declares the backend set expected by a
  package build. Add `vulkan` or `cuda` only when the packaging recipe supplies
  the matching system ggml backend package.

For example, a repository build that expects optional Vulkan and CUDA
subpackage recipes can configure:

```sh
cmake -S app -B build-distro \
  -DAI_FILE_SORTER_DISTRO_REPOSITORY_BUILD=ON \
  -DAI_FILE_SORTER_UPDATE_MODE=DISABLED \
  -DAI_FILE_SORTER_LLAMA_PROVIDER=system \
  -DAI_FILE_SORTER_LLAMA_BACKENDS=cpu,vulkan,cuda \
  -DAI_FILE_SORTER_PDF_BACKEND=pdftotext
```

With the `system` llama provider, the app links against package-managed
`llama`, `ggml`, and `mtmd` development files. CUDA and Vulkan are runtime ggml
backend packages rather than libraries bundled into the AIFS package.

## Windows naming and migration note

New registry/settings/integration paths should use `HFStudio`. Compatibility
code may still need to read or clean legacy `Quicknode` locations during
migration or uninstall flows.

## Related references

- [Headless runtime contract](headless-runtime-contract.md)
- [Updater contract](updater-contract.md)
- [Windows release builds](windows-release-builds.md)
