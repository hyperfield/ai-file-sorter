# macOS Intel ad-hoc re-signing after load-command edits

Commits covered: `ae7bd3a`

## Summary

This commit fixed a macOS Intel packaging problem caused by mutating Mach-O binaries with `install_name_tool` and then leaving them unsigned. It added a small helper script that re-signs modified binaries with an ad-hoc signature and wired that helper into both the llama runtime build flow and the main install flow.

## Motivation

On macOS, `install_name_tool` changes invalidate an existing code signature. The repository already adjusts install names and RPATHs for staged runtime libraries and for the installed executable. That is normally survivable during development, but Intel builds had become sensitive to these post-link mutations and could fail later because the modified artifacts were no longer consistently signed.

## Implementation

The new helper script is intentionally narrow:

```bash
codesign --force --sign - --timestamp=none "$target"
```

It performs ad-hoc signing only, skips symlink aliases, and is used only on Darwin.

The Makefile and the llama build helper now call that script after `install_name_tool` mutates:

- staged `libpdfium.dylib`
- the installed `aifilesorter` executable
- Intel llama runtime dylibs after RPATH cleanup

The Intel-only guard matters. Apple Silicon builds were left alone because the observed breakage was specific to the x86_64 path.

## Validation

No new automated tests were added. Validation for this change is build- and launch-oriented:

- the generated Intel runtime dylibs remain signed after RPATH normalization
- the installed executable is re-signed after its RPATH is edited

## User-visible impact

The practical effect is that native Intel macOS builds are less likely to fail late because a binary was modified after signing. This is especially relevant for local packaging and install flows that stage runtime libraries under the app-specific install location.

## Remaining caveats

- The signing here is ad-hoc, not notarization or Developer ID signing.
- The fix only addresses the invalid-signature problem introduced by local binary mutation; it does not create a full macOS distribution pipeline by itself.
