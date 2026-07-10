# Built-in Documents whitelist preset

Commit covered: `dc86f7a`

## Why this change was justified

Whitelists are most useful when users can start from a sensible preset instead of building every vocabulary from scratch. The app already had a default whitelist story, but document-heavy workflows benefit from a narrower and more purposeful built-in preset.

That is what this commit added: a seeded `Documents` whitelist that appears alongside the default setup when no whitelists exist yet.

## What changed

The whitelist store’s bootstrap logic now creates a built-in documents-oriented preset instead of forcing users to author one manually.

```cpp
// Seed a built-in documents-focused whitelist for common office/archive workflows.
if (lists.empty()) {
    create_default_whitelist();
    create_documents_whitelist();
}
```

The important product motivation is that document analysis and rename/categorization flows were already present in the app. Adding a documents-specific whitelist closes the loop by giving users a ready-made vocabulary that matches those capabilities.

## Why this matters for prompt quality

A good whitelist does more than constrain output. It improves prompt focus, especially for smaller local models.

With a document-oriented preset:

- categories stay closer to likely office/document structures
- the user spends less time curating a whitelist before the first run
- document categorization becomes easier to steer toward a stable taxonomy

This is especially relevant in a product that already encourages narrower whitelists for better prompt behavior.

## Test coverage

The commit also added test coverage to verify that the seeded preset is actually present and usable rather than being only a UI expectation.

```cpp
// Representative test intent: ensure the built-in documents preset exists on bootstrap.
REQUIRE(whitelist_store.contains("Documents"));
```

That matters because bootstrap data often regresses quietly if only the UI is inspected manually.

## Net effect

After this change:

- first-run users get a ready-made `Documents` whitelist
- document-oriented categorization is easier to constrain immediately
- the built-in presets are more aligned with the app’s actual feature surface

This was a small commit, but it improved the “first useful run” experience for document-heavy folders.
