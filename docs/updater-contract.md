# Updater Contract

This page summarizes the update-feed and live-test contract used by the app's
updater.

## Feed shape

The updater reads a top-level `update` object. Newer clients can read
per-platform streams from:

- `update.windows`
- `update.macos`
- `update.linux`

Older clients may still depend on the flat compatibility fields directly under
`update`, so keep those fields populated if you still support older versions.

## Common stream fields

- `current_version`
- `min_version`
- `download_url`
- `changelog`

`changelog` should be a JSON array of strings when you want bullet-style update
notes in the dialog.

## Windows installer fields

Windows streams may also provide:

- `installer_url`
- `installer_sha256`

When both fields are present, the app can download, verify, and launch the
installer directly. `installer_url` may point to an `.exe`, an `.msi`, or a ZIP
archive containing exactly one installer payload.

## Feed selection

- `UPDATE_SPEC_FILE_URL` is the normal feed URL.
- `UPDATE_SPEC_FILE_URL_DEVELOPMENT` is preferred when the app starts with
  `--development`.
- If the development-specific value is unset, development mode falls back to
  `UPDATE_SPEC_FILE_URL`.

## Windows updater live-test mode

The Windows launcher supports:

- `--updater-live-test`
- `--updater-live-test-url=<url>`
- `--updater-live-test-sha256=<sha256>`
- `--updater-live-test-version=<version>`
- `--updater-live-test-min-version=<version>`

If live-test mode is enabled and the URL or SHA value is omitted on the command
line, the launcher also checks for `live-test.ini` next to the executable.
Command-line flags still override values read from `live-test.ini`.

## Compatibility rules

- Keep the flat `update.current_version`, `update.min_version`, and
  `update.download_url` fields if you need one generic compatibility stream for
  older app versions.
- Newer clients prefer the per-platform streams when present.
- The legacy flat stream cannot represent different per-platform installers or
  version lines cleanly, so platform streams should be considered the primary
  shape going forward.
