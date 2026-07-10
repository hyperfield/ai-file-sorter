# Storage Plugin Dialog Translations

## Summary

Commit `a341651` completed unfinished `StoragePluginDialog` translations in all supported Qt translation catalogs.

## Motivation

The storage plugin management dialog had untranslated strings in the `.ts` catalogs. That left the translation validation script failing and meant users in non-English UI languages could still see English labels, actions, and error messages in this dialog.

## Implementation

The commit filled translations for the storage plugin dialog in:

- French
- German
- Italian
- Spanish
- Dutch
- Turkish
- Korean

The translated strings cover dialog title text, install/update/uninstall actions, plugin status labels, file filters, and failure messages.

## Validation

The translation validation was rerun after the catalog update:

```bash
tests/run_translation_tests.sh
```

The Qt test target was also rebuilt during the implementation sequence so regenerated `.qm` files reported all translations as finished.

## User-visible impact

Users running the app in a translated UI language should see localized storage plugin management text instead of English fallback strings.

## Remaining caveats

Translation quality should still be reviewed by native speakers before release, especially for terminology such as "plugin", "storage plugin", and package/source wording.
