# 2025-10-28: chore(resources): update

## Covered commits
- `eb187d8` `2025-10-28` `chore(resources): update`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/resources/app.qrc`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/resources/app.qrc`. It changed the repository support state, metadata, or supporting files in the way described by `chore(resources): update`.

Before this commit, the repository reflected the state immediately preceding `eb187d8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/resources/app.qrc b/app/resources/app.qrc
--- a/app/resources/app.qrc
+++ b/app/resources/app.qrc
@@ -5,5 +5,6 @@
     <file>images/app_icon_128.png</file>
     <file>images/icon_512x512.png</file>
     <file>.env</file>
+    <file>certs/cacert.pem</file>
   </qresource>
 </RCC>
```

The excerpt is taken from the commit diff for `chore(resources): update`. The most relevant surfaces are `app/resources/app.qrc`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
