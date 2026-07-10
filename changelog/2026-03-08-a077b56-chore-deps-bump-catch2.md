# 2026-03-08: chore(deps): bump Catch2

## Covered commits
- `a077b56` `2026-03-08` `chore(deps): bump Catch2`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `external/Catch2`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `external/Catch2`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `a077b56`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/external/Catch2 b/external/Catch2
--- a/external/Catch2
+++ b/external/Catch2
@@ -1 +1 @@
-Subproject commit b59f4f352291a89c4a3fe1488eeccbddeea85625
+Subproject commit de7e8630134f46e67a6b59269436f9cab94cd28e
```

The excerpt is taken from the commit diff for `chore(deps): bump Catch2`. The most relevant surfaces are `external/Catch2`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
