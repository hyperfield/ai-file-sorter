# 2025-05-28: chore: stop tracking built binary

## Covered commits
- `90ef8b1` `2025-05-28` `chore: stop tracking built binary`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `D` `app/bin/aifilesorter`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/bin/aifilesorter`. It changed the repository support state, metadata, or supporting files in the way described by `chore: stop tracking built binary`.

Before this commit, the repository reflected the state immediately preceding `90ef8b1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/bin/aifilesorter b/app/bin/aifilesorter
```

The excerpt is taken from the commit diff for `chore: stop tracking built binary`. The most relevant surfaces are `app/bin/aifilesorter`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
