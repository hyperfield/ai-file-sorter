# Donation Dialog Copy and Routing

## Summary

Commits `170c560`, `3742ae9`, and `630b46a` clean up the donation/support flow by clarifying the translatable wording and routing the action to the correct website destination.

## Motivation

Small support and donation dialogs are easy to treat as low-priority copy, but they still matter:

- the wording appears in multiple languages
- users expect the action to land on the correct support page
- inconsistent source strings make translation upkeep harder than it should be

## Implementation

The sequence did three things:

- clarified the donation-related source strings
- aligned the translatable source text so catalogs could stay consistent
- routed the support action to the correct `filesorter.app` donation endpoint

## Validation

Validation was mainly UI- and translation-oriented:

- confirm the action opens the intended website location
- confirm translated catalogs match the new source strings

## User-visible impact

Users see clearer support/donation wording, and the action should lead to the right website location instead of an outdated or indirect target.

## Remaining caveats

This is intentionally narrow UI polish. It improves correctness and translation hygiene, but it does not change the broader support model or fundraising presentation.
