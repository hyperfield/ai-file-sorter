# Document undo and local learning in help

## Summary

The local Quick Start guide now explains safer review/apply behavior, Undo, and local learning.

## Motivation

The help text needed to better match the app’s current workflow: AI analyzes and suggests, the app applies file operations only after approval, and users can undo applied organization actions.

## Implementation

- Reworded the Quick Start introduction to emphasize review and approval.
- Added a user-friendly explanation that the AI drives analysis but does not directly touch files.
- Added an Undo section.
- Added local learning guidance explaining that approved review decisions can improve future category suggestions without training the LLM.
- Updated localized help content alongside the English guide.

## Validation

Help resources were reviewed as Markdown content and included in the subsequent build/test pass for the feature track.

## User-visible impact

Users get clearer in-app guidance about what the AI does, when files are changed, how Undo fits into the workflow, and how local learning works.

## Remaining caveats

This updates the local app help. Website FAQ updates remain a separate publishing concern.
