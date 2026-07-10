# Release 1.8.0 documentation summary

Commits covered: `80d4cae`

## Summary

This commit updated the repository-facing release notes for `1.8.0`. The code version badge in `README.md`, the short changelog bullets in `README.md`, and the fuller release section in `CHANGELOG.md` were all brought in line with the new storage-plugin and OneDrive work that had landed just before this point.

## Motivation

The project had gained a real plugin-management flow and a dedicated OneDrive storage plugin, but the top-level release notes still described the older state of the product. That left a gap between what the app could do and what the docs claimed.

This commit also added `/changelog/` to `.gitignore`, which made sense if the long-form chapter files were being treated as local working notes, but it also means new chapter files under that directory do not appear in `git status` unless they are force-added or the ignore rule is relaxed.

## Implementation

The implementation was documentation-only:

- the README badge moved from `1.7.3` to `1.8.0`
- `README.md` gained short bullets for plugin installation and dedicated OneDrive support
- `CHANGELOG.md` gained a `1.8.0` section summarizing the plugin system, remote catalog flow, and OneDrive support
- `.gitignore` was updated to ignore `/changelog/`

## Validation

Validation was limited to documentation review. No code paths or automated tests changed in this commit.

## User-visible impact

Users reading the repository docs now see `1.8.0` described accurately:

- plugin installation/update/removal is called out explicitly
- `.aifsplugin` archives and remote plugin catalogs are mentioned
- the OneDrive plugin is presented as a first-class capability rather than an implementation detail

## Remaining caveats

- This commit did not change runtime behavior; it only documented behavior that had already landed.
- Because `/changelog/` is ignored, future chapter files under that directory require special handling if they are meant to be tracked in Git.
