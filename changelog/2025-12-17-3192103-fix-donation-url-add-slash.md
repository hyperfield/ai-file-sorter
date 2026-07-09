# 2025-12-17: fix(donation-url): add slash

## Covered commits
- `3192103` `2025-12-17` `fix(donation-url): add slash`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/MainAppHelpActions.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/include/external/llama.cpp`, `app/lib/MainAppHelpActions.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(donation-url): add slash`.

Before this commit, the repository reflected the state immediately preceding `3192103`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 3f3a4fb9c3b907c68598363b204e6f58f4757c8c
+Subproject commit ee09828cb057460b369576410601a3a09279e23c
diff --git a/app/lib/MainAppHelpActions.cpp b/app/lib/MainAppHelpActions.cpp
index 75808d1..65c7f79 100644
--- a/app/lib/MainAppHelpActions.cpp
+++ b/app/lib/MainAppHelpActions.cpp
@@ -123,6 +123,6 @@ void MainAppHelpActions::show_agpl_info(QWidget* parent)
 
 void MainAppHelpActions::open_support_page()
 {
-    const QUrl donation_url(QStringLiteral("https://filesorter.app/donate"));
+    const QUrl donation_url(QStringLiteral("https://filesorter.app/donate/"));
     QDesktopServices::openUrl(donation_url);
 }
```

The excerpt is taken from the commit diff for `fix(donation-url): add slash`. The most relevant surfaces are `app/include/external/llama.cpp`, `app/lib/MainAppHelpActions.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
