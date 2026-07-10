# 2025-02-06: Updated year in About

## Covered commits
- `1b016a9` `2025-02-06` `Updated year in About`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/MainAppHelpActions.cpp`

## What changed from what, why, and how
The commit modified `app/lib/MainAppHelpActions.cpp`. It changed the repository from the prior state to the state described by `Updated year in About`.

Before this commit, the repository reflected the state immediately preceding `1b016a9`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/MainAppHelpActions.cpp b/app/lib/MainAppHelpActions.cpp
--- a/app/lib/MainAppHelpActions.cpp
+++ b/app/lib/MainAppHelpActions.cpp
@@ -45,7 +45,7 @@ void MainAppHelpActions::show_about(GtkWindow *parent)
     std::string version_text = "Version: " + APP_VERSION.to_string();
     GtkWidget *version = gtk_label_new(version_text.c_str());
 
-    GtkWidget *copyright = gtk_label_new("© 2024 QuickNode. All rights reserved.");
+    GtkWidget *copyright = gtk_label_new("© 2024-2025 QuickNode. All rights reserved.");
     GtkWidget *website = gtk_link_button_new_with_label("https://www.filesorter.app", "Visit the Website");
 
     gtk_box_pack_start(GTK_BOX(about_tab_content), program_name, FALSE, FALSE, 5);
```

The excerpt is taken from the commit diff for `Updated year in About`. The most relevant surfaces are `app/lib/MainAppHelpActions.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
