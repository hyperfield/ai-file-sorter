# Update test catalog

## Summary

`TESTS.md` was updated to document the current test coverage and commands.

## Motivation

New learning, categorization, help, and runtime-stability tests were added during the feature work. The project test catalog needed to reflect those additions so future validation is easier to run and audit.

## Implementation

- Documented the new learned-behavior and categorization consistency test coverage.
- Updated the relevant test commands and descriptions.
- Kept developer-facing validation notes aligned with the current test suite.

## Validation

The documented tests were checked against the current test build and command flow.

## User-visible impact

No direct end-user change. Developers and maintainers have a more accurate test reference.

## Remaining caveats

`TESTS.md` must continue to be updated whenever tests are added, removed, or renamed.
