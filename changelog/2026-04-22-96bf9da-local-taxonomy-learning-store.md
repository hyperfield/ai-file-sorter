# Add local taxonomy learning store

## Summary

The app now has a separate local learning database for user-approved taxonomy behavior.

## Motivation

Learned categorization behavior should not be mixed with temporary categorization cache data. Users may want to clear caches without deleting the category preferences the app learned from their reviews.

## Implementation

- Added a dedicated `user_learning` store with schema versioning from the start.
- Added persistence for approved taxonomy entries, file examples, aliases, embedding metadata, and embedding vectors.
- Added whitelist import support so existing user-defined category lists can seed the learning store.
- Kept the learning store separate from the existing cache-clearing path.

## Validation

Targeted persistence and learning-store tests were added and run. The implementation keeps schema creation and migration behavior explicit so future learning data can be upgraded safely.

## User-visible impact

The app can start building a local, user-owned categorization memory without training or modifying the LLM itself.

## Remaining caveats

This is the storage foundation. Higher-quality retrieval and ranking behavior is built on top of this store in later commits.
