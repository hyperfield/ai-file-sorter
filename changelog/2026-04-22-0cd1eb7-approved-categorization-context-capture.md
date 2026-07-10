# Capture approved categorization context

## Summary

The review flow now records approved file-to-category decisions with richer context.

## Motivation

Learning only a final category label is weak. Future retrieval needs enough context to recognize similar files, including filename, extension, path hints, and available analysis text.

## Implementation

- Captured approved category and subcategory mappings from the review dialog.
- Stored useful file context such as filename, extension, parent path hints, suggested name, and available document or image analysis context.
- Avoided teaching the learning store from dry-run operations.
- Kept learning tied to user-approved review decisions rather than raw AI suggestions.

## Validation

Added and ran targeted tests for approved mapping capture and learning-store persistence.

## User-visible impact

The app can learn from what users actually approve, improving future suggestions while keeping the user in control.

## Remaining caveats

The quality of captured context depends on the metadata and analysis available for each file.
