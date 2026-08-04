# Summary

This documentation update added explicit "safe first run" guidance to the README and the in-app quick-start guide, then added test coverage to make sure the English quick-start text keeps that onboarding advice.

# Motivation

AI File Sorter is safer when people start with a small, disposable folder and review the results before touching a larger archive. That advice existed informally, but it was important enough to become part of the first-run documentation rather than tribal knowledge.

# Implementation

The README gained a new Safe First Run section, the localized quick-start guide gained matching English guidance, and the translation-oriented help tests were updated to assert that the English quick-start resource still contains the safe-first-run messaging.

# Validation

Validation came from the quick-start translation test updates in `tests/unit/test_main_app_translation.cpp` and the related `TESTS.md` documentation.

# User-visible impact

First-time users now get stronger in-product guidance to try the app on a small test folder before running it against a large archive or drive.

# Remaining caveats

This chapter only changes documentation and onboarding text. It does not enforce a staged first run programmatically.
