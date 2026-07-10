# Summary

This small docs-and-metadata series added GitHub sponsorship metadata and rewrote the README donation copy so support is framed as voluntary project funding rather than a paywall. The merge commit `9ee25d1` is included here because it is the point where the funding metadata landed on `dev`.

# Motivation

The project already had a support dialog and donation flow, but the repository metadata and README text did not explain the support model clearly enough. In particular, the documentation needed to say plainly that local processing, privacy, undo, and preview features remain available without a donation.

# Implementation

Two pieces changed together:

- `.github/FUNDING.yml` added sponsorship metadata for the repository
- `README.md` clarified suggested support levels and explicitly stated that donation codes only hide the reminder instead of unlocking core features

This was deliberately messaging work, not application-logic work.

# Validation

Validation was limited to documentation review and repository metadata inspection.

# User-visible impact

Users reading the README or the repository sidebar now get a clearer explanation of what financial support does and does not change in the app.

# Remaining caveats

This chapter did not change the support dialog code itself. It aligned the public-facing messaging around it.
