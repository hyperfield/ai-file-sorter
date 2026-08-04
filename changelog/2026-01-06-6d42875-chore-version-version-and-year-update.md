# 2026-01-06: chore(version): version and year update

## Covered commits
- `6d42875` `2026-01-06` `chore(version): version and year update`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/app_version.hpp`
- `M` `app/lib/MainAppHelpActions.cpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `app/include/app_version.hpp`, `app/lib/MainAppHelpActions.cpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `6d42875`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/app_version.hpp b/app/include/app_version.hpp
--- a/app/include/app_version.hpp
+++ b/app/include/app_version.hpp
@@ -3,4 +3,4 @@
 #include "Version.hpp"
 
 
-const Version APP_VERSION = Version{1, 4, 5};
+const Version APP_VERSION = Version{1, 5, 0};
diff --git a/app/lib/MainAppHelpActions.cpp b/app/lib/MainAppHelpActions.cpp
index 65c7f79..ec19b13 100644
--- a/app/lib/MainAppHelpActions.cpp
+++ b/app/lib/MainAppHelpActions.cpp
@@ -44,7 +44,7 @@ void MainAppHelpActions::show_about(QWidget* parent)
     about_layout->addWidget(version_label);
 
     auto* copyright_label =
-        new QLabel(QStringLiteral("© 2024-2025 QuickNode. All rights reserved."), about_tab);
+        new QLabel(QStringLiteral("© 2024-2026 QuickNode. All rights reserved."), about_tab);
     copyright_label->setAlignment(Qt::AlignHCenter);
     about_layout->addWidget(copyright_label);
```

The excerpt is taken from the commit diff for `chore(version): version and year update`. The most relevant surfaces are `app/include/app_version.hpp`, `app/lib/MainAppHelpActions.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
