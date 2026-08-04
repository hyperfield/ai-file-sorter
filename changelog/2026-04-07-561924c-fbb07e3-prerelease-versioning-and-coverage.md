# Prerelease version parsing and comparison

Commits covered: `561924c`, `fbb07e3`

## Summary

This commit pair taught the app to understand prerelease version labels such as `beta`, switched the project version to `1.8.0 beta`, and added dedicated unit coverage for parsing and ordering behavior.

## Motivation

The old `Version` logic only understood dot-separated integers. That was not enough once the app needed to advertise a prerelease like `1.8.0 beta` while still letting the updater compare:

- stable releases against prereleases
- skipped versions against feed versions
- synthesized live-test versions against the current app version

Without explicit prerelease support, comparisons and formatting would drift into string hacks or incorrect ordering.

## Implementation

The `Version` type gained:

- `Version::parse(...)`
- `has_prerelease()`
- `prerelease_tag()`
- `to_numeric_string()`
- a shared `compare(...)` implementation

The comparison rule is intentionally simple and repository-specific:

```cpp
if (lhs_tag.empty()) {
    return 1;
}
if (rhs_tag.empty()) {
    return -1;
}
```

A stable version sorts above the same numeric version with a prerelease tag. That is the key behavior the updater needed.

The updater stopped using its own version-string parser and now relies on `Version::parse(...)`. It also uses `to_numeric_string()` when synthesizing a fake “newer” version for updater live tests, so `1.8.0 beta` does not become a malformed `1.8.0 beta.1`.

The direct tests added in `fbb07e3` cover:

- formatting with and without prerelease tags
- parsing `1.8.0 beta`, `1.8.0-beta`, and compact tags like `1.8.0b1`
- ordering stable releases above prereleases
- malformed numeric component rejection

## Validation

Validation is stronger than usual for a versioning change because the tests are explicit and dedicated:

- `tests/unit/test_version.cpp` was added
- the test target was updated in `app/CMakeLists.txt`

## User-visible impact

- The app version now displays as `1.8.0 beta`.
- The updater can compare prerelease and stable releases sanely.
- Skipped-version and live-test update logic no longer rely on lossy string behavior.

## Remaining caveats

- Prerelease tags are ordered lexicographically once the numeric digits match; this is simpler than full Semantic Versioning precedence rules.
- This is enough for the app’s current `beta` use case, but more elaborate tag families would need additional ordering policy later.
