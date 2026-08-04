# 2026-01-16: chore(support-dialog): update donation link

## Covered commits
- `7e40b86` `2026-01-16` `chore(support-dialog): update donation link`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/MainAppHelpActions.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/MainAppHelpActions.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(support-dialog): update donation link`.

Before this commit, the repository reflected the state immediately preceding `7e40b86`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/MainAppHelpActions.cpp b/app/lib/MainAppHelpActions.cpp
--- a/app/lib/MainAppHelpActions.cpp
+++ b/app/lib/MainAppHelpActions.cpp
@@ -123,6 +123,6 @@ void MainAppHelpActions::show_agpl_info(QWidget* parent)
 
 void MainAppHelpActions::open_support_page()
 {
-    const QUrl donation_url(QStringLiteral("https://filesorter.app/donate/"));
+    const QUrl donation_url(QStringLiteral("https://www.paypal.com/donate/?hosted_button_id=Z3XYTG38C62HQ"));
     QDesktopServices::openUrl(donation_url);
 }
```

The excerpt is taken from the commit diff for `chore(support-dialog): update donation link`. The most relevant surfaces are `app/lib/MainAppHelpActions.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
