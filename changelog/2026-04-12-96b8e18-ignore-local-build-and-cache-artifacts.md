# Ignore local build and cache artifacts

Commits covered: `96b8e18`

## Summary

This commit expanded `.gitignore` to cover a broader set of local build outputs and machine-specific clutter.

## Motivation

Recent work had touched more than one build system and more than one local workflow:

- CMake and Ninja
- Qt Creator / IDE work
- Python helpers
- local SQLite scratch databases

Without broader ignore coverage, local diagnostics and one-off builds could leave a noisy worktree that obscured real source changes.

## Implementation

The new ignore rules cover:

- defensive CMake/Ninja outputs such as `CMakeCache.txt`, `CMakeFiles/`, `compile_commands.json`, and `.ninja_*`
- IDE / OS noise such as `.idea/`, `Thumbs.db`, and `Desktop.ini`
- Python caches outside the already-known script paths
- local `*.sqlite` scratch files

## Validation

Validation is operational rather than automated: after this commit, common local build and cache artifacts stop appearing in `git status`.

## User-visible impact

There is no runtime effect. The benefit is repository hygiene for developers and maintainers.

## Remaining caveats

- Broader ignore coverage can hide files that are interesting locally, so contributors still need judgment when deciding whether a new generated artifact should eventually be tracked.
