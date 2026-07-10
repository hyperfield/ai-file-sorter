# Summary

This commit added missing translations for the `Checking local backend...` status text across the non-English UI catalogs that were already shipping with the app.

# Motivation

The status string had been introduced in the UI, but the translation catalogs lagged behind it. That creates uneven startup and benchmark messaging in localized builds, where one of the most visible runtime-status lines can suddenly fall back to English.

# Implementation

The change was intentionally narrow: it updated the `.ts` catalogs for the supported interface languages that were missing a translation entry for the local-backend status message.

# Validation

Validation was catalog-based rather than code-based. The important outcome was that the message now exists in the shipped translation resources instead of relying on an English fallback.

# User-visible impact

Users running the localized UI are more likely to see the local-backend status line in their selected interface language during startup or local-backend checks.

# Remaining caveats

This commit only covers the missing string entry. It does not change when the status is shown or how backend probing behaves.
