# 2026-03-22: 1.7.3 release paperwork, CI scoping, submodule refresh, and documentation updates

## Covered commits
- `f64b528` `2026-03-22` `chore(ci): run build workflow only on pull requests`
- `4364620` `2026-03-22` `chore(release): bump version to 1.7.3 and update changelog`
- `094cb8b` `2026-03-22` `chore(llama): bump embedded llama.cpp submodule`
- `81ed1e4` `2026-03-22` `chore(submodules): update third-party pointers`
- `5f1ccb9` `2026-03-22` `docs(screenshots): refresh Windows and French UI captures`
- `7855334` `2026-03-22` `chore(documents):  update`
- `fd6cca4` `2026-03-22` `chore(docs): update`

## Motivation
Once the functional work for 1.7.3 landed, the release still required operational cleanup: the build workflow scope needed tightening, submodule pointers and screenshots had to be refreshed, and the README/changelog had to reflect the final shipped behavior rather than the in-flight state from earlier in the week.

## What changed
This grouped release-support chapter covers the CI trigger change, version/changelog bump, llama.cpp and other submodule pointer updates, refreshed screenshots, and the final documentation passes that prepared the repository for the 1.7.3 cut.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `f64b528`
```diff
diff --git a/.github/workflows/build.yml b/.github/workflows/build.yml
--- a/.github/workflows/build.yml
+++ b/.github/workflows/build.yml
@@ -1,8 +1,6 @@
 name: Build
 
 on:
-  push:
-    branches: [ main, dev ]
   pull_request:
     branches: [ main, dev ]
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `4364620`
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,7 +1,8 @@
 ﻿# Changelog
 
-## [1.7.2] - 2026-03-22
+## [1.7.3] - 2026-03-22
 
+- Non-English categorization is now more reliable: files are categorized canonically in English first, then translated into the selected category language, with localized labels persisted separately from the canonical taxonomy/cache.
 - App updates now support separate update streams for Windows, macOS, and Linux, while still accepting the legacy single-stream manifest format for newer clients.
 - Windows feeds can now provide a direct installer URL plus SHA-256 checksum so the app can download the installer, show download progress, verify its integrity, and launch it after confirmation.
 - The UI translation system was migrated fully to Qt `.ts` / `.qm` catalogs, and missing translations for all currently supported interface languages were filled in.
@@ -11,6 +12,8 @@
 - macOS local-LLM packaging/runtime handling was hardened: bundled llama/ggml dylibs are now relocatable, and the app no longer falls back to conflicting system/Homebrew ggml libraries during backend loading.
 - Linux/macOS build and packaging flows were improved, including staged PDFium runtime files, better Debian package dependencies, CPU/CUDA/Vulkan Debian package variants, and improved Homebrew MediaInfo detection on macOS source builds.
 - Added cross-platform diagnostics collection scripts for Linux, macOS, and Windows.
+- Misc improvements.
+- Misc bug fixes.
 
 ## [1.7.0] - 2026-03-08
```

This second excerpt is included because `4364620` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
