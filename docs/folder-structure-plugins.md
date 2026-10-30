# Folder Structure Plugins

Folder-structure plugins are declarative `.aifsplugin` ZIP archives. They add starter templates and extra recognition/routing guidance for existing folder trees without shipping executable code.

## Package Layout

Each archive must contain exactly one `manifest.json`. The manifest may be at the archive root or under one top-level package directory. The app treats the manifest's parent directory as the package root.

Required package files:

```text
manifest.json
plugin-signature.json
plugin-signature.sig
profiles/<profile>.json
```

`manifest.json`:

```json
{
  "id": "johnny_decimal_support",
  "name": "Johnny.Decimal Support",
  "description": "Adds Johnny.Decimal structure guidance.",
  "version": "1.0.0",
  "entry_point_kind": "folder_structure_profile",
  "platforms": ["windows", "macos", "linux"],
  "architectures": ["x86_64", "arm64"],
  "profile_paths": ["profiles/johnny.json"]
}
```

Profile JSON:

```json
{
  "id": "johnny_decimal_profile",
  "name": "Johnny.Decimal Complete",
  "description": "Templates and routing rules for a Johnny.Decimal archive.",
  "structure_kind": "johnny_decimal",
  "detectors": ["johnny_decimal_like"],
  "initial_directories": [
    "00-09 System",
    "00-09 System/01 Index",
    "10-19 Admin",
    "10-19 Admin/11 Finance"
  ],
  "prompt_guidance": [
    "Keep Johnny.Decimal IDs attached to folder names when routing into an existing archive."
  ],
  "new_folder_guidance": [
    "When creating a child folder, use the next free number inside the parent range."
  ]
}
```

Supported detector keys in the current app-side wiring:

```text
johnny_decimal_like
numbered_prefix
alphabetic_prefix
```

## Signing

The app verifies folder-structure plugins with Ed25519. This is tamper-evident: if any signed payload file is changed after signing, the package is rejected at install time and on later loads.

`plugin-signature.json` is the exact byte payload that is signed:

```json
{
  "schema_version": 1,
  "algorithm": "ed25519",
  "key_id": "vendor-key-2026-01",
  "files": [
    {
      "path": "manifest.json",
      "sha256": "<hex sha256>"
    },
    {
      "path": "profiles/johnny.json",
      "sha256": "<hex sha256>"
    }
  ]
}
```

`plugin-signature.sig` must contain the Ed25519 signature over the exact bytes of `plugin-signature.json`. The signature file may contain either raw 64-byte signature data or Base64/Base64URL text.

Every non-signature payload file must be listed in `plugin-signature.json`, and `manifest.json` must be listed. The app rejects unsigned payload files and executable/script-like payloads, including `.exe`, `.dll`, `.msi`, `.ps1`, `.bat`, `.cmd`, `.sh`, `.py`, `.js`, `.vbs`, `.so`, and `.dylib`.

Trusted public keys are compiled into the app with the CMake cache variable:

```powershell
cmake -S app -B build `
  -DAI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_PUBLIC_KEYS="vendor-key-2026-01:<base64-ed25519-public-key>"
```

Multiple keys can be separated with commas or semicolons.
