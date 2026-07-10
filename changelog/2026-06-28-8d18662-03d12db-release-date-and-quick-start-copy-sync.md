# Summary

These documentation commits synchronized two visible text surfaces with the current product state: the 1.8.0 release date in the short-form release notes and the quick-start/help text for review modes in the localized help files.

# Motivation

When release notes and in-app help drift from the UI, users end up reconciling old wording with the current product behavior. That is especially noticeable in the quick-start guide because it is meant to be read during first use.

# Implementation

`CHANGELOG.md` and `README.md` were corrected to use the right 1.8.0 date, while the localized quick-start markdown files were updated so the mode labels match the current UI wording instead of older terminology.

# Validation

Validation was straightforward documentation review. The quick-start files are shipped resources, so keeping them aligned with UI copy is itself the key compatibility concern.

# User-visible impact

Users see more accurate release notes and help text, especially when reading the localized quick-start guide before their first run.

# Remaining caveats

This chapter only adjusted documentation and localized help resources. It did not change the underlying review-mode logic.
